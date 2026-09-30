#include <output/WorkBufferPool.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <protocols/types/Buffer.hpp>
#include <render/Framebuffer.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <memory>

using namespace std::chrono_literals;

namespace Monitor {
    struct SWorkBufferStats {
        int  created        = 0;
        int  allocated      = 0;
        int  destroyed      = 0;
        int  prepared       = 0;
        bool failAllocation = false;
        bool failCreation   = false;
    };

    class CWorkBufferTexture : public Render::ITexture {
      public:
        void setTexParameter(GLenum, GLint) override {
            ;
        }
        void allocate(const Vector2D&, uint32_t) override {
            ;
        }
        void update(uint32_t, uint8_t*, uint32_t, const CRegion&) override {
            ;
        }
    };

    class CWorkBufferFramebuffer : public Render::IFramebuffer {
      public:
        CWorkBufferFramebuffer(SP<SWorkBufferStats> stats) : m_stats(stats) {
            ++m_stats->created;
        }
        ~CWorkBufferFramebuffer() override {
            ++m_stats->destroyed;
        }
        void release() override {
            m_fbAllocated = false;
            m_tex.reset();
            m_size = {};
        }
        bool readPixels(CHLBufferReference, uint32_t, uint32_t, uint32_t, uint32_t) override {
            return false;
        }
        void bind() override {
            ;
        }
        void addStencil(SP<Render::ITexture> texture) override {
            m_stencilTex = texture;
        }

      private:
        bool internalAlloc(int, int, DRMFormat) override {
            ++m_stats->allocated;
            if (m_stats->failAllocation)
                return false;
            m_tex = makeShared<CWorkBufferTexture>();
            return true;
        }

        SP<SWorkBufferStats> m_stats;
    };

    class CWorkBufferPoolTest : public ::testing::Test {
      protected:
        void SetUp() override {
            m_previousEventLoop = std::move(g_pEventLoopManager);
            m_loop.reset(wl_event_loop_create());
            ASSERT_TRUE(m_loop);
            g_pEventLoopManager = makeUnique<CEventLoopManager>(nullptr, m_loop.get());
        }

        void TearDown() override {
            g_pEventLoopManager.reset();
            m_loop.reset();
            g_pEventLoopManager = std::move(m_previousEventLoop);
        }

        UP<CWorkBufferPool> pool(uint64_t limit = CWorkBufferPool::BYTE_LIMIT) {
            return makeUnique<CWorkBufferPool>(
                [stats = m_stats]() -> SP<Render::IFramebuffer> {
                    if (stats->failCreation)
                        return nullptr;
                    return makeShared<CWorkBufferFramebuffer>(stats);
                },
                [stats = m_stats] { ++stats->prepared; }, limit);
        }

        void age(CWorkBufferPool& pool) {
            for (auto& resource : pool.m_resources)
                resource.idleSince = Time::steadyNow() - 6s;
        }

        void tick(CWorkBufferPool& pool) {
            ASSERT_TRUE(pool.m_expiryTimer);
            pool.m_expiryTimer->updateTimeout(-1s);
            g_pEventLoopManager->onTimerFire();
        }

        SP<CEventLoopTimer> timer(CWorkBufferPool& pool) {
            return pool.m_expiryTimer;
        }

        size_t count(const CWorkBufferPool& pool) {
            return pool.m_resources.size();
        }

        bool idleObserved(const CWorkBufferPool& pool) {
            return pool.m_resources.at(0).idleSince.has_value();
        }

        SP<SWorkBufferStats> m_stats = makeShared<SWorkBufferStats>();

      private:
        std::unique_ptr<wl_event_loop, decltype(&wl_event_loop_destroy)> m_loop{nullptr, wl_event_loop_destroy};
        UP<CEventLoopManager>                                            m_previousEventLoop;
    };

    TEST_F(CWorkBufferPoolTest, ReusesExactSizeWithoutAllocation) {
        auto cache  = pool();
        auto buffer = cache->acquire({100, 200}, DRM_FORMAT_ARGB8888, nullptr);
        ASSERT_TRUE(buffer);
        WP<Render::IFramebuffer> original = buffer;
        buffer.reset();

        auto reused = cache->acquire({100, 200}, DRM_FORMAT_ARGB8888, nullptr);
        EXPECT_EQ(reused, original.lock());
        EXPECT_EQ(m_stats->created, 1);
        EXPECT_EQ(m_stats->allocated, 1);
    }

    TEST_F(CWorkBufferPoolTest, SerialSizesKeepBoundedWarmCache) {
        auto cache = pool();
        for (int size = 100; size < 120; ++size) {
            auto buffer = cache->acquire({size, size}, DRM_FORMAT_ARGB8888, nullptr);
            ASSERT_TRUE(buffer);
            EXPECT_EQ(buffer->m_size, Vector2D(size, size));
        }
        EXPECT_EQ(m_stats->created, 8);
        EXPECT_EQ(count(*cache), 8);
    }

    TEST_F(CWorkBufferPoolTest, AlternatingSizesAllocateOnlyDuringWarmup) {
        auto cache = pool();
        ASSERT_TRUE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        ASSERT_TRUE(cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, nullptr));
        ASSERT_EQ(m_stats->allocated, 2);

        for (int i = 0; i < 20; ++i) {
            ASSERT_TRUE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
            ASSERT_TRUE(cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, nullptr));
        }
        EXPECT_EQ(m_stats->allocated, 2);
        EXPECT_EQ(m_stats->created, 2);
        EXPECT_EQ(count(*cache), 2);
    }

    TEST_F(CWorkBufferPoolTest, PrefersExactMatchToResizingOtherIdleBuffer) {
        auto                     cache  = pool();
        auto                     first  = cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr);
        auto                     second = cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, nullptr);
        WP<Render::IFramebuffer> exact  = second;
        first.reset();
        second.reset();

        auto reused = cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, nullptr);
        EXPECT_EQ(reused, exact.lock());
        EXPECT_EQ(m_stats->allocated, 2);
    }

    TEST_F(CWorkBufferPoolTest, ConcurrentUseHonorsEntryLimitAndResizesOnlyIdleEntry) {
        auto                                  cache = pool();
        std::vector<SP<Render::IFramebuffer>> held;
        for (int i = 0; i < 8; ++i) {
            held.emplace_back(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
            ASSERT_TRUE(held.back());
        }
        EXPECT_FALSE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        WP<Render::IFramebuffer> released = held.back();
        held.pop_back();
        auto reused = cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, nullptr);
        EXPECT_EQ(reused, released.lock());
        EXPECT_EQ(m_stats->created, 8);
    }

    TEST_F(CWorkBufferPoolTest, BudgetPreservesFourByteBaselineAndCountsFP16AsEightBytes) {
        auto cache = pool();
        EXPECT_TRUE(cache->acquire({8192, 8192}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_TRUE(cache->acquire({8192, 8192}, DRM_FORMAT_XRGB2101010, nullptr));
        EXPECT_FALSE(cache->acquire({8192, 8192}, DRM_FORMAT_ABGR16161616F, nullptr));
        EXPECT_FALSE(cache->acquire({8192, 4097}, DRM_FORMAT_ABGR16161616F, nullptr));
        EXPECT_TRUE(cache->acquire({8192, 4096}, DRM_FORMAT_ABGR16161616F, nullptr));
        EXPECT_EQ(m_stats->created, 1);
    }

    TEST_F(CWorkBufferPoolTest, BusyAllocationsStayAccountedAndCannotBeEvicted) {
        auto cache  = pool();
        auto first  = cache->acquire({8192, 4096}, DRM_FORMAT_ARGB8888, nullptr);
        auto second = cache->acquire({8192, 4096}, DRM_FORMAT_ARGB8888, nullptr);
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        age(*cache);
        tick(*cache);
        EXPECT_FALSE(cache->acquire({1, 1}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_EQ(count(*cache), 2);
        EXPECT_EQ(m_stats->destroyed, 0);

        second.reset();
        auto replacement = cache->acquire({4096, 4096}, DRM_FORMAT_ABGR16161616F, nullptr);
        EXPECT_TRUE(replacement);
        EXPECT_EQ(first->m_size, Vector2D(8192, 4096));
        EXPECT_EQ(m_stats->created, 2);
    }

    TEST_F(CWorkBufferPoolTest, EvictsOnlyIdleEntriesToFitByteBudget) {
        auto                                  cache = pool();
        auto                                  held  = cache->acquire({4096, 4096}, DRM_FORMAT_ARGB8888, nullptr);
        std::vector<SP<Render::IFramebuffer>> idle;
        for (int i = 0; i < 3; ++i)
            idle.emplace_back(cache->acquire({4096, 4096}, DRM_FORMAT_ARGB8888, nullptr));
        idle.clear();

        EXPECT_TRUE(cache->acquire({12288, 4096}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_EQ(count(*cache), 2);
        EXPECT_EQ(m_stats->created, 4);
        EXPECT_EQ(m_stats->destroyed, 2);
        EXPECT_EQ(held->m_size, Vector2D(4096, 4096));
    }

    TEST_F(CWorkBufferPoolTest, ImpossibleRequestDoesNotEvictIdleEntries) {
        auto cache = pool();
        auto held  = cache->acquire({8192, 4096}, DRM_FORMAT_ARGB8888, nullptr);
        ASSERT_TRUE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_FALSE(cache->acquire({8192, 8192}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_EQ(count(*cache), 2);
        EXPECT_EQ(m_stats->destroyed, 0);
    }

    TEST_F(CWorkBufferPoolTest, AllocationFailuresDoNotPoisonCacheOrAccounting) {
        auto cache            = pool();
        m_stats->failCreation = true;
        EXPECT_FALSE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        m_stats->failCreation   = false;
        m_stats->failAllocation = true;
        EXPECT_FALSE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_EQ(count(*cache), 0);

        m_stats->failAllocation = false;
        // Fill the byte budget so the next miss must resize this idle allocation.
        ASSERT_TRUE(cache->acquire({8192, 8192}, DRM_FORMAT_ARGB8888, nullptr));
        m_stats->failAllocation = true;
        EXPECT_FALSE(cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_EQ(count(*cache), 0);
        EXPECT_EQ(m_stats->destroyed, 2);

        m_stats->failAllocation = false;
        EXPECT_TRUE(cache->acquire({8192, 8192}, DRM_FORMAT_ARGB8888, nullptr));
    }

    TEST_F(CWorkBufferPoolTest, RejectsInvalidDimensionsAndFormatsBeforeAllocation) {
        auto cache = pool();
        for (const auto& size : std::vector<Vector2D>{
                 {0, 100},
                 {-1, 100},
                 {100, 0},
                 {100, -1},
                 {0.5, 100.0},
                 {100.0, 1.5},
                 {std::numeric_limits<double>::quiet_NaN(), 100.0},
                 {100.0, std::numeric_limits<double>::infinity()},
                 {sc<double>(std::numeric_limits<int>::max()) + 1, 100.0},
                 {100.0, sc<double>(std::numeric_limits<int>::max()) + 1},
                 {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
                 {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()},
             })
            EXPECT_FALSE(cache->acquire(size, DRM_FORMAT_ABGR16161616F, nullptr));
        EXPECT_FALSE(cache->acquire({100, 100}, DRM_FORMAT_INVALID, nullptr));
        auto unbounded = pool(std::numeric_limits<uint64_t>::max());
        EXPECT_FALSE(unbounded->acquire({std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}, DRM_FORMAT_ABGR16161616F, nullptr));
        EXPECT_EQ(m_stats->created, 0);
    }

    TEST_F(CWorkBufferPoolTest, TimerExpiresIdleBuffersWithoutFurtherRequestsAndRearmsAfterReuse) {
        auto                     cache    = pool();
        auto                     buffer   = cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr);
        WP<Render::IFramebuffer> original = buffer;
        buffer.reset();
        tick(*cache);
        EXPECT_TRUE(idleObserved(*cache));
        age(*cache);
        tick(*cache);

        EXPECT_TRUE(original.expired());
        EXPECT_EQ(m_stats->prepared, 1);
        EXPECT_EQ(count(*cache), 0);
        EXPECT_FALSE(timer(*cache)->armed());

        EXPECT_TRUE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_TRUE(timer(*cache)->armed());
    }

    TEST_F(CWorkBufferPoolTest, HeldBufferGetsFullIdleGracePeriodAfterRelease) {
        auto cache = pool();
        auto held  = cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr);
        age(*cache);
        tick(*cache);
        EXPECT_EQ(count(*cache), 1);
        EXPECT_FALSE(idleObserved(*cache));
        EXPECT_EQ(m_stats->prepared, 0);

        held.reset();
        tick(*cache);
        EXPECT_TRUE(idleObserved(*cache));
        EXPECT_EQ(count(*cache), 1);
        age(*cache);
        tick(*cache);
        EXPECT_EQ(count(*cache), 0);
    }

    TEST_F(CWorkBufferPoolTest, ReacquisitionResetsIdleAge) {
        auto cache = pool();
        ASSERT_TRUE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        age(*cache);
        ASSERT_TRUE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        tick(*cache);
        EXPECT_EQ(count(*cache), 1);
        EXPECT_EQ(m_stats->destroyed, 0);
    }

    TEST_F(CWorkBufferPoolTest, DestructionCancelsTimerEvenWithOtherTimerReferences) {
        auto cache = pool();
        ASSERT_TRUE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        auto expiry = timer(*cache);
        expiry->updateTimeout(-1s);
        cache.reset();
        EXPECT_TRUE(expiry->cancelled());
        EXPECT_FALSE(expiry->armed());
        EXPECT_EQ(m_stats->destroyed, 1);
        g_pEventLoopManager->onTimerFire();
        EXPECT_EQ(m_stats->prepared, 1);
    }

    TEST_F(CWorkBufferPoolTest, DestructionAfterEventLoopTeardownIsSafe) {
        auto cache = pool();
        ASSERT_TRUE(cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr));
        auto expiry = timer(*cache);
        g_pEventLoopManager.reset();
        cache.reset();
        EXPECT_TRUE(expiry->cancelled());
        EXPECT_EQ(m_stats->destroyed, 1);
    }

    TEST_F(CWorkBufferPoolTest, ImageDescriptionChangesPropagateToHeldAndIdleBuffers) {
        auto cache = pool();
        auto held  = cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION);
        auto idle  = cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION);
        ASSERT_TRUE(held);
        ASSERT_TRUE(idle);
        WP<Render::IFramebuffer> cached = idle;
        idle.reset();

        cache->setImageDescription(NColorManagement::DEFAULT_HDR_IMAGE_DESCRIPTION);
        EXPECT_EQ(held->imageDescription(), NColorManagement::DEFAULT_HDR_IMAGE_DESCRIPTION);
        ASSERT_FALSE(cached.expired());
        EXPECT_EQ(cached.lock()->imageDescription(), NColorManagement::DEFAULT_HDR_IMAGE_DESCRIPTION);

        auto reused = cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, NColorManagement::DEFAULT_HDR_IMAGE_DESCRIPTION);
        EXPECT_EQ(reused, cached.lock());
        EXPECT_EQ(m_stats->allocated, 2);
    }

    TEST_F(CWorkBufferPoolTest, UnusedIterationSkipsHeldBuffers) {
        auto cache = pool();
        auto held  = cache->acquire({100, 100}, DRM_FORMAT_ARGB8888, nullptr);
        ASSERT_TRUE(cache->acquire({200, 200}, DRM_FORMAT_ARGB8888, nullptr));
        int visited = 0;
        cache->forEachUnused([&](SP<Render::IFramebuffer> buffer) {
            EXPECT_EQ(buffer->m_size, Vector2D(200, 200));
            ++visited;
        });
        EXPECT_EQ(visited, 1);
    }
}
