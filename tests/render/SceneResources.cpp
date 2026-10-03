#include <render/Context.hpp>
#include <render/Framebuffer.hpp>
#include <render/SceneResources.hpp>
#include <protocols/types/Buffer.hpp>

#include <gtest/gtest.h>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace Render {
    struct SSceneResourceStats {
        int  allocated      = 0;
        int  destroyed      = 0;
        bool failAllocation = false;
    };

    class CSceneResourceTexture : public ITexture {
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

    class CSceneResourceFramebuffer : public IFramebuffer {
      public:
        explicit CSceneResourceFramebuffer(SP<SSceneResourceStats> stats) : m_stats(stats) {
            ;
        }
        ~CSceneResourceFramebuffer() override {
            ++m_stats->destroyed;
        }
        void release() override {
            m_fbAllocated = false;
            m_tex.reset();
            m_size = {};
        }
        bool readPixels(CHLBufferReference, uint32_t, uint32_t, uint32_t, uint32_t) override {
            ADD_FAILURE() << "Unexpected framebuffer readback";
            return false;
        }
        void bind() override {
            ADD_FAILURE() << "Unexpected framebuffer bind";
        }
        void addStencil(SP<ITexture> texture) override {
            m_stencilTex = texture;
        }

      private:
        bool internalAlloc(int, int, DRMFormat) override {
            ++m_stats->allocated;
            if (m_stats->failAllocation)
                return false;
            m_tex = makeShared<CSceneResourceTexture>();
            return true;
        }

        SP<SSceneResourceStats> m_stats;
    };

    class CSceneResourcesTest : public ::testing::Test {
      protected:
        SP<SSceneResourceStats> m_stats       = makeShared<SSceneResourceStats>();
        SP<IFramebuffer>        m_framebuffer = makeShared<CSceneResourceFramebuffer>(m_stats);
        SP<CSceneResources>     m_resources   = makeShared<CSceneResources>(m_framebuffer);
    };

    TEST_F(CSceneResourcesTest, InitialAndPreparedContentsRemainInvalidUntilCompleted) {
        EXPECT_TRUE(m_resources->isolated());
        EXPECT_EQ(m_resources->blurFramebuffer(), m_framebuffer);
        EXPECT_TRUE(m_resources->blurDirty());
        EXPECT_FALSE(m_resources->blurQueued());
        EXPECT_FALSE(m_resources->blurTexture());
        EXPECT_FALSE(m_resources->canPrecomputeBlur());
        EXPECT_EQ(m_stats->allocated, 0);

        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION));
        ASSERT_TRUE(m_framebuffer->getTexture());
        EXPECT_TRUE(m_resources->canPrecomputeBlur());
        EXPECT_FALSE(m_resources->blurTexture());
        EXPECT_TRUE(m_resources->blurDirty());
        EXPECT_EQ(m_framebuffer->imageDescription(), NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION);

        m_resources->setBlurQueued(true);
        EXPECT_TRUE(m_resources->blurQueued());
        EXPECT_FALSE(m_resources->blurTexture());
        m_resources->completePreBlur();
        EXPECT_EQ(m_resources->blurTexture(), m_framebuffer->getTexture());
        EXPECT_FALSE(m_resources->blurDirty());
        EXPECT_FALSE(m_resources->blurQueued());
        EXPECT_TRUE(m_resources->canPrecomputeBlur());
    }

    TEST_F(CSceneResourcesTest, IdenticalDescriptorsDoNotSharePixelsOrFlags) {
        auto            otherFramebuffer = makeShared<CSceneResourceFramebuffer>(m_stats);
        CSceneResources other{otherFramebuffer};
        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        ASSERT_TRUE(other.prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_EQ(m_stats->allocated, 2);
        EXPECT_NE(m_resources->blurFramebuffer(), other.blurFramebuffer());
        EXPECT_NE(m_framebuffer->getTexture(), otherFramebuffer->getTexture());

        m_resources->setBlurQueued(true);
        EXPECT_FALSE(other.blurQueued());
        other.setBlurQueued(true);
        m_resources->completePreBlur();
        EXPECT_TRUE(m_resources->blurTexture());
        EXPECT_FALSE(other.blurTexture());
        EXPECT_TRUE(other.blurDirty());
        EXPECT_TRUE(other.blurQueued());

        other.completePreBlur();
        ASSERT_TRUE(other.blurTexture());
        m_resources->setBlurDirty(true);
        EXPECT_FALSE(m_resources->blurTexture());
        EXPECT_EQ(other.blurTexture(), otherFramebuffer->getTexture());
        EXPECT_FALSE(other.blurDirty());
        EXPECT_FALSE(other.blurQueued());
    }

    TEST_F(CSceneResourcesTest, NextPrepareReusesAllocationButRequiresNewContents) {
        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        m_resources->completePreBlur();
        const auto texture = m_resources->blurTexture();
        ASSERT_TRUE(texture);
        m_resources->setBlurQueued(true);

        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_EQ(m_stats->allocated, 1);
        EXPECT_EQ(m_framebuffer->getTexture(), texture);
        EXPECT_FALSE(m_resources->blurTexture());
        EXPECT_TRUE(m_resources->blurDirty());
        EXPECT_FALSE(m_resources->blurQueued());

        m_resources->setBlurDirty(false);
        EXPECT_FALSE(m_resources->blurTexture());
        m_resources->completePreBlur();
        EXPECT_EQ(m_resources->blurTexture(), texture);
    }

    TEST_F(CSceneResourcesTest, DirtyingCompletedContentsRequiresAnotherCompletion) {
        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        m_resources->completePreBlur();
        const auto texture = m_resources->blurTexture();
        ASSERT_TRUE(texture);
        m_resources->setBlurDirty(true);
        EXPECT_TRUE(m_resources->blurDirty());
        EXPECT_FALSE(m_resources->blurTexture());
        m_resources->setBlurDirty(false);
        EXPECT_FALSE(m_resources->blurTexture());
        m_resources->completePreBlur();
        EXPECT_EQ(m_resources->blurTexture(), texture);
        EXPECT_EQ(m_stats->allocated, 1);
    }

    TEST_F(CSceneResourcesTest, SizeFormatAndDescriptionChangesInvalidateContents) {
        struct SDescriptor {
            Vector2D                            size;
            DRMFormat                           format = DRM_FORMAT_INVALID;
            NColorManagement::PImageDescription description;
            int                                 allocations = 0;
        };
        const std::vector<SDescriptor> descriptors = {
            {{100, 200}, DRM_FORMAT_ARGB8888, NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION, 1},
            {{200, 100}, DRM_FORMAT_ARGB8888, NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION, 2},
            {{200, 100}, DRM_FORMAT_XRGB2101010, NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION, 3},
            {{200, 100}, DRM_FORMAT_XRGB2101010, NColorManagement::DEFAULT_HDR_IMAGE_DESCRIPTION, 3},
            {{200, 100}, DRM_FORMAT_XRGB2101010, nullptr, 3},
        };
        for (const auto& descriptor : descriptors) {
            SCOPED_TRACE(descriptor.allocations);
            m_resources->setBlurQueued(true);
            ASSERT_TRUE(m_resources->prepare(descriptor.size, descriptor.format, descriptor.description));
            EXPECT_FALSE(m_resources->blurTexture());
            EXPECT_TRUE(m_resources->blurDirty());
            EXPECT_FALSE(m_resources->blurQueued());
            EXPECT_EQ(m_framebuffer->m_size, descriptor.size);
            EXPECT_EQ(m_framebuffer->m_drmFormat, descriptor.format);
            EXPECT_EQ(m_framebuffer->imageDescription(), descriptor.description);
            EXPECT_EQ(m_stats->allocated, descriptor.allocations);
            m_resources->completePreBlur();
            EXPECT_EQ(m_resources->blurTexture(), m_framebuffer->getTexture());
            ASSERT_TRUE(m_resources->blurTexture());
            EXPECT_EQ(m_resources->blurTexture()->m_imageDescription, descriptor.description);
        }
    }

    TEST_F(CSceneResourcesTest, FailedAllocationHidesPreviousContentsAndCanRecover) {
        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        m_resources->completePreBlur();
        const auto previous = m_resources->blurTexture();
        ASSERT_TRUE(previous);
        m_resources->setBlurQueued(true);
        m_stats->failAllocation = true;

        EXPECT_FALSE(m_resources->prepare({200, 100}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_FALSE(m_resources->canPrecomputeBlur());
        EXPECT_FALSE(m_resources->blurTexture());
        EXPECT_TRUE(m_resources->blurDirty());
        EXPECT_FALSE(m_resources->blurQueued());
        EXPECT_FALSE(m_framebuffer->isAllocated());
        m_resources->completePreBlur();
        EXPECT_FALSE(m_resources->blurTexture());
        EXPECT_TRUE(m_resources->blurDirty());

        m_stats->failAllocation = false;
        ASSERT_TRUE(m_resources->prepare({200, 100}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_TRUE(m_resources->canPrecomputeBlur());
        EXPECT_FALSE(m_resources->blurTexture());
        m_resources->completePreBlur();
        ASSERT_TRUE(m_resources->blurTexture());
        EXPECT_NE(m_resources->blurTexture(), previous);
        EXPECT_EQ(m_stats->allocated, 3);
    }

    TEST_F(CSceneResourcesTest, InvalidDimensionsHidePreviousContentsWithoutAllocation) {
        for (const auto& size : std::vector<Vector2D>{
                 {0, 200},
                 {100, 0},
                 {-1, 200},
                 {100, -1},
                 {0.5, 200.0},
                 {100.0, 0.5},
                 {100.5, 200.0},
                 {100.0, 200.5},
                 {std::numeric_limits<double>::quiet_NaN(), 200.0},
                 {100.0, std::numeric_limits<double>::quiet_NaN()},
                 {std::numeric_limits<double>::infinity(), 200.0},
                 {100.0, std::numeric_limits<double>::infinity()},
                 {-std::numeric_limits<double>::infinity(), 200.0},
                 {100.0, -std::numeric_limits<double>::infinity()},
                 {sc<double>(std::numeric_limits<int>::max()) + 1, 200.0},
                 {100.0, sc<double>(std::numeric_limits<int>::max()) + 1},
                 {std::numeric_limits<double>::max(), 200.0},
                 {100.0, std::numeric_limits<double>::max()},
             }) {
            SCOPED_TRACE(::testing::Message() << size.x << "x" << size.y);
            ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
            m_resources->completePreBlur();
            const auto previous = m_resources->blurTexture();
            ASSERT_TRUE(previous);
            m_resources->setBlurQueued(true);

            EXPECT_FALSE(m_resources->prepare(size, DRM_FORMAT_ARGB8888, nullptr));
            EXPECT_FALSE(m_resources->canPrecomputeBlur());
            EXPECT_FALSE(m_resources->blurTexture());
            EXPECT_TRUE(m_resources->blurDirty());
            EXPECT_FALSE(m_resources->blurQueued());
            EXPECT_EQ(m_stats->allocated, 1);
            EXPECT_EQ(m_framebuffer->getTexture(), previous);
            m_resources->completePreBlur();
            EXPECT_FALSE(m_resources->blurTexture());
            EXPECT_TRUE(m_resources->blurDirty());
            m_resources->setBlurDirty(false);
            EXPECT_FALSE(m_resources->blurTexture());
        }
    }

    TEST(SceneResources, MissingIsolatedFramebufferNeverExposesContents) {
        CSceneResources resources{SP<IFramebuffer>{}};
        EXPECT_TRUE(resources.isolated());
        EXPECT_FALSE(resources.blurFramebuffer());
        EXPECT_FALSE(resources.prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_FALSE(resources.canPrecomputeBlur());
        EXPECT_TRUE(resources.blurDirty());
        resources.completePreBlur();
        EXPECT_FALSE(resources.blurTexture());
        EXPECT_TRUE(resources.blurDirty());
    }

    TEST_F(CSceneResourcesTest, IsolatedWorkBufferDetachesMirrorWithoutChangingPrivateBlurCache) {
        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        m_resources->completePreBlur();
        m_resources->setBlurQueued(true);
        const auto cacheTexture = m_resources->blurTexture();
        auto       work         = makeShared<CSceneResourceFramebuffer>(m_stats);
        ASSERT_TRUE(work->alloc(100, 200));
        auto mirror = makeShared<CSceneResourceTexture>();
        work->enableMirror(mirror);
        ASSERT_EQ(work->getMirrorTexture(), mirror);
        const auto allocations = m_stats->allocated;

        EXPECT_EQ(m_resources->prepareWorkBuffer(work), work);
        EXPECT_TRUE(work->isAllocated());
        EXPECT_FALSE(work->getMirrorTexture());
        EXPECT_EQ(m_stats->allocated, allocations + 1);
        EXPECT_EQ(m_resources->prepareWorkBuffer(work), work);
        EXPECT_EQ(m_stats->allocated, allocations + 1);
        EXPECT_EQ(m_resources->blurFramebuffer(), m_framebuffer);
        EXPECT_EQ(m_resources->blurTexture(), cacheTexture);
        EXPECT_TRUE(m_resources->canPrecomputeBlur());
        EXPECT_FALSE(m_resources->blurDirty());
        EXPECT_TRUE(m_resources->blurQueued());
    }

    TEST_F(CSceneResourcesTest, MonitorWorkBufferKeepsMirrorWithoutReallocation) {
        CSceneResources monitorResources{PHLMONITORREF{}};
        auto            work = makeShared<CSceneResourceFramebuffer>(m_stats);
        ASSERT_TRUE(work->alloc(100, 200));
        auto mirror = makeShared<CSceneResourceTexture>();
        work->enableMirror(mirror);
        const auto texture     = work->getTexture();
        const auto allocations = m_stats->allocated;

        EXPECT_EQ(monitorResources.prepareWorkBuffer(work), work);
        EXPECT_EQ(work->getMirrorTexture(), mirror);
        EXPECT_EQ(work->getTexture(), texture);
        EXPECT_EQ(m_stats->allocated, allocations);
    }

    TEST_F(CSceneResourcesTest, FailedMirrorDetachmentReturnsNoUsableWorkBuffer) {
        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        m_resources->completePreBlur();
        const auto cacheTexture = m_resources->blurTexture();
        auto       work         = makeShared<CSceneResourceFramebuffer>(m_stats);
        ASSERT_TRUE(work->alloc(100, 200));
        work->enableMirror(makeShared<CSceneResourceTexture>());
        const auto allocations  = m_stats->allocated;
        m_stats->failAllocation = true;

        EXPECT_FALSE(m_resources->prepareWorkBuffer(work));
        EXPECT_FALSE(work->getMirrorTexture());
        EXPECT_FALSE(work->isAllocated());
        EXPECT_EQ(m_stats->allocated, allocations + 1);
        EXPECT_FALSE(m_resources->prepareWorkBuffer(work));
        EXPECT_EQ(m_stats->allocated, allocations + 1);
        EXPECT_EQ(m_resources->blurTexture(), cacheTexture);
        EXPECT_TRUE(m_resources->canPrecomputeBlur());
        EXPECT_FALSE(m_resources->blurDirty());
    }

    TEST_F(CSceneResourcesTest, NullAndUnallocatedWorkBuffersStayUnusable) {
        auto work = makeShared<CSceneResourceFramebuffer>(m_stats);
        for (const auto& resources : {m_resources, makeShared<CSceneResources>(PHLMONITORREF{})}) {
            EXPECT_FALSE(resources->prepareWorkBuffer(nullptr));
            EXPECT_FALSE(resources->prepareWorkBuffer(work));
        }
        EXPECT_EQ(m_stats->allocated, 0);
        EXPECT_FALSE(work->isAllocated());
        EXPECT_FALSE(m_resources->canPrecomputeBlur());
        EXPECT_TRUE(m_resources->blurDirty());
        EXPECT_FALSE(m_resources->blurQueued());
    }

    TEST_F(CSceneResourcesTest, ContextResetPreservesExternallyOwnedResources) {
        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        m_resources->completePreBlur();
        const auto texture = m_resources->blurTexture();
        ASSERT_TRUE(texture);
        CRenderContext context;
        ASSERT_TRUE(context.begin(m_resources));
        EXPECT_EQ(context.sceneResources(), m_resources);
        context.reset();
        EXPECT_FALSE(context.active());
        EXPECT_FALSE(context.sceneResources());
        EXPECT_FALSE(context.readOnlyEffects());
        EXPECT_EQ(m_resources->blurTexture(), texture);
        EXPECT_FALSE(m_resources->blurDirty());
        EXPECT_EQ(m_stats->destroyed, 0);

        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        ASSERT_TRUE(context.begin(m_resources));
        EXPECT_EQ(context.sceneResources(), m_resources);
        EXPECT_FALSE(context.sceneResources()->blurTexture());
        EXPECT_EQ(m_framebuffer->getTexture(), texture);
        EXPECT_EQ(m_stats->allocated, 1);
    }

    TEST_F(CSceneResourcesTest, ContextRetainsOwnerUntilResetOrDestruction) {
        WP<CSceneResources> owner       = m_resources;
        WP<IFramebuffer>    framebuffer = m_framebuffer;
        {
            CRenderContext context;
            ASSERT_TRUE(context.begin(m_resources));
            m_resources.reset();
            m_framebuffer.reset();
            EXPECT_FALSE(owner.expired());
            EXPECT_FALSE(framebuffer.expired());
            context.reset();
            EXPECT_TRUE(owner.expired());
            EXPECT_TRUE(framebuffer.expired());
            EXPECT_EQ(m_stats->destroyed, 1);

            ASSERT_TRUE(context.begin(makeShared<CSceneResources>(makeShared<CSceneResourceFramebuffer>(m_stats))));
            owner       = context.sceneResources();
            framebuffer = context.sceneResources()->blurFramebuffer();
        }
        EXPECT_TRUE(owner.expired());
        EXPECT_TRUE(framebuffer.expired());
        EXPECT_EQ(m_stats->destroyed, 2);
    }

    TEST_F(CSceneResourcesTest, IsolatedFrameTimeIsStableAndRejectedBeginKeepsSelection) {
        CRenderContext context;
        const auto     before = Time::steadyNow();
        ASSERT_TRUE(context.begin(m_resources));
        const auto frameTime = context.effectTime();
        EXPECT_GE(frameTime, before);
        EXPECT_LE(frameTime, Time::steadyNow());
        EXPECT_TRUE(context.readOnlyEffects());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        EXPECT_EQ(context.effectTime(), frameTime);

        auto other = makeShared<CSceneResources>(makeShared<CSceneResourceFramebuffer>(m_stats));
        EXPECT_FALSE(context.begin(other));
        EXPECT_FALSE(context.begin());
        EXPECT_TRUE(context.active());
        EXPECT_EQ(context.sceneResources(), m_resources);
        EXPECT_TRUE(context.readOnlyEffects());
        EXPECT_EQ(context.effectTime(), frameTime);

        context.reset();
        const auto nextFrame = Time::steadyNow();
        ASSERT_TRUE(context.begin(other));
        EXPECT_EQ(context.sceneResources(), other);
        EXPECT_GE(context.effectTime(), nextFrame);
        EXPECT_GT(context.effectTime(), frameTime);
        EXPECT_LE(context.effectTime(), Time::steadyNow());
    }

    TEST_F(CSceneResourcesTest, NestedDrawStateKeepsOwnerRoutingAndFrameTime) {
        CRenderContext context, other;
        ASSERT_TRUE(context.begin(m_resources));
        auto otherResources = makeShared<CSceneResources>(makeShared<CSceneResourceFramebuffer>(m_stats));
        ASSERT_TRUE(other.begin(otherResources));
        const auto frameTime  = context.effectTime();
        context.m_data.fbSize = {100, 200};
        {
            auto outer            = context.saveDrawState();
            context.m_data        = {};
            context.m_data.fbSize = {300, 400};
            {
                auto inner     = context.saveDrawState();
                context.m_data = {};
                EXPECT_EQ(context.sceneResources(), m_resources);
                EXPECT_EQ(context.effectTime(), frameTime);
                EXPECT_TRUE(context.readOnlyEffects());
                context.sceneResources()->setBlurQueued(true);
                EXPECT_FALSE(other.sceneResources()->blurQueued());
            }
            EXPECT_EQ(context.m_data.fbSize, Vector2D(300, 400));
            EXPECT_EQ(context.sceneResources(), m_resources);
            EXPECT_EQ(context.effectTime(), frameTime);
        }
        EXPECT_EQ(context.m_data.fbSize, Vector2D(100, 200));
        EXPECT_EQ(context.sceneResources(), m_resources);
        EXPECT_EQ(context.effectTime(), frameTime);
        EXPECT_TRUE(context.active());
        EXPECT_TRUE(context.readOnlyEffects());
        EXPECT_EQ(other.sceneResources(), otherResources);
    }

    TEST_F(CSceneResourcesTest, NestedDrawScopesRestoreQueueButKeepCompletedContents) {
        for (const bool queued : {false, true}) {
            SCOPED_TRACE(queued);
            ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
            m_resources->setBlurQueued(queued);
            CRenderContext context;
            ASSERT_TRUE(context.begin(m_resources));
            {
                auto outer = context.saveDrawState();
                m_resources->setBlurQueued(!queued);
                {
                    auto inner = context.saveDrawState();
                    m_resources->completePreBlur();
                    EXPECT_FALSE(m_resources->blurQueued());
                    EXPECT_FALSE(m_resources->blurDirty());
                    EXPECT_EQ(m_resources->blurTexture(), m_framebuffer->getTexture());
                }
                EXPECT_EQ(m_resources->blurQueued(), !queued);
                EXPECT_FALSE(m_resources->blurDirty());
                EXPECT_EQ(m_resources->blurTexture(), m_framebuffer->getTexture());
            }
            EXPECT_EQ(m_resources->blurQueued(), queued);
            EXPECT_FALSE(m_resources->blurDirty());
            EXPECT_EQ(m_resources->blurTexture(), m_framebuffer->getTexture());
        }
    }

    TEST_F(CSceneResourcesTest, ExceptionUnwindsQueuesWithoutRollingBackCompletionOrInvalidation) {
        ASSERT_TRUE(m_resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        m_resources->setBlurQueued(true);
        CRenderContext context;
        ASSERT_TRUE(context.begin(m_resources));
        EXPECT_THROW(
            {
                auto outer = context.saveDrawState();
                m_resources->setBlurQueued(false);
                try {
                    auto inner = context.saveDrawState();
                    m_resources->completePreBlur();
                    m_resources->setBlurQueued(true);
                    throw std::runtime_error("Injected nested draw exception");
                } catch (const std::runtime_error&) {
                    EXPECT_FALSE(m_resources->blurQueued());
                    EXPECT_FALSE(m_resources->blurDirty());
                    EXPECT_EQ(m_resources->blurTexture(), m_framebuffer->getTexture());
                    throw;
                }
            },
            std::runtime_error);
        EXPECT_TRUE(m_resources->blurQueued());
        EXPECT_FALSE(m_resources->blurDirty());
        EXPECT_EQ(m_resources->blurTexture(), m_framebuffer->getTexture());

        EXPECT_THROW(
            {
                auto scope = context.saveDrawState();
                m_resources->setBlurQueued(false);
                m_resources->setBlurDirty(true);
                throw std::runtime_error("Injected invalidating draw exception");
            },
            std::runtime_error);
        EXPECT_TRUE(m_resources->blurQueued());
        EXPECT_TRUE(m_resources->blurDirty());
        EXPECT_FALSE(m_resources->blurTexture());
        EXPECT_TRUE(m_resources->canPrecomputeBlur());
    }

    TEST(SceneResources, DefaultContextUsesLiveEffectPolicy) {
        CRenderContext context;
        EXPECT_FALSE(context.sceneResources());
        EXPECT_FALSE(context.readOnlyEffects());
        ASSERT_TRUE(context.begin());
        EXPECT_FALSE(context.sceneResources());
        EXPECT_FALSE(context.readOnlyEffects());
        const auto initial = context.effectTime();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto before  = Time::steadyNow();
        const auto current = context.effectTime();
        EXPECT_GT(current, initial);
        EXPECT_GE(current, before);
        EXPECT_LE(current, Time::steadyNow());
    }

    TEST(SceneResources, NullMonitorAdapterRemainsNonIsolated) {
        auto resources = makeShared<CSceneResources>(PHLMONITORREF{});
        EXPECT_FALSE(resources->isolated());
        EXPECT_FALSE(resources->prepare({100, 200}, DRM_FORMAT_ARGB8888, nullptr));
        EXPECT_FALSE(resources->blurFramebuffer());
        EXPECT_FALSE(resources->blurTexture());
        resources->setBlurDirty(true);
        resources->setBlurQueued(true);
        EXPECT_FALSE(resources->blurDirty());
        EXPECT_FALSE(resources->blurQueued());
        resources->completePreBlur();
        EXPECT_FALSE(resources->isolated());
        EXPECT_FALSE(resources->blurFramebuffer());
        EXPECT_FALSE(resources->blurTexture());

        CRenderContext context;
        ASSERT_TRUE(context.begin(resources));
        EXPECT_EQ(context.sceneResources(), resources);
        EXPECT_FALSE(context.readOnlyEffects());
    }
}
