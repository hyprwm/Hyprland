#include <render/Context.hpp>
#include <Compositor.hpp>
#include <event/EventBus.hpp>
#include <helpers/sync/SyncReleaser.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/core/Compositor.hpp>

#include <gtest/gtest.h>
#include <array>
#include <cerrno>
#include <memory>
#include <stdexcept>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace Render;
using namespace Hyprutils::OS;

class CAsyncUseTestBuffer : public IHLBuffer {
  public:
    Aquamarine::eBufferCapability caps() override {
        return Aquamarine::BUFFER_CAPABILITY_NONE;
    }
    Aquamarine::eBufferType type() override {
        return Aquamarine::BUFFER_TYPE_MISC;
    }
    void update(const CRegion&) override {
        ADD_FAILURE();
    }
    bool isSynchronous() override {
        return false;
    }
    bool good() override {
        return true;
    }
    void lock() override {
        ++m_locks;
        IHLBuffer::lock();
    }
    void unlock() override {
        --m_locks;
        IHLBuffer::unlock();
    }
    void sendRelease() override {
        ++m_releases;
        IHLBuffer::sendRelease();
    }

    int m_locks    = 0;
    int m_releases = 0;
};

// Real buffer locks and Wayland release events, with eventfds standing in for GPU fences.
class CSurfaceBufferUseTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        std::array<int, 2> sockets = {-1, -1};
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        CFileDescriptor server{sockets[0]};
        m_socket = CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), server.get());
        ASSERT_NE(m_client, nullptr);
        server.take();

        m_previousCompositor       = std::move(g_pCompositor);
        m_previousEventLoop        = std::move(g_pEventLoopManager);
        m_previousEventBus         = std::move(Event::bus());
        m_previousPresentation     = std::move(PROTO::presentation);
        m_installedGlobals         = true;
        Event::bus()               = makeUnique<Event::CEventBus>();
        g_pCompositor              = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay = m_display.get();
        g_pEventLoopManager        = makeUnique<CEventLoopManager>(m_display.get(), wl_display_get_event_loop(m_display.get()));
        PROTO::presentation        = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "buffer-use-test");
    }

    void TearDown() override {
        if (m_installedGlobals) {
            g_pEventLoopManager = std::move(m_previousEventLoop);
            PROTO::presentation = std::move(m_previousPresentation);
            g_pCompositor       = std::move(m_previousCompositor);
            Event::bus()        = std::move(m_previousEventBus);
        }
        if (m_display)
            wl_display_destroy_clients(m_display.get());
    }

    SP<CAsyncUseTestBuffer> buffer() {
        auto buffer                  = makeShared<CAsyncUseTestBuffer>();
        auto wire                    = makeShared<CWlBuffer>(m_client, 1, m_nextID++);
        buffer->m_resource           = CWLBufferResource::create(wire);
        buffer->m_resource->m_buffer = buffer;
        return buffer;
    }

    SP<CWLSurfaceResource> surface() {
        auto surface    = makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(m_client, 6, m_nextID++));
        surface->m_self = surface;
        return surface;
    }

    void dispatch() {
        ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    }

    void signal(const CFileDescriptor& fd) {
        ASSERT_EQ(eventfd_write(fd.get(), 1), 0);
        dispatch();
    }

    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    wl_client*                                                 m_client = nullptr;

  private:
    uint32_t                  m_nextID           = 2;
    bool                      m_installedGlobals = false;
    UP<CCompositor>           m_previousCompositor;
    UP<CEventLoopManager>     m_previousEventLoop;
    UP<Event::CEventBus>      m_previousEventBus;
    UP<CPresentationProtocol> m_previousPresentation;
};

TEST_F(CSurfaceBufferUseTest, DeduplicatesSurfaceBufferPairAndSendsRealRelease) {
    auto           first = surface(), second = surface();
    auto           a = buffer(), b = buffer();
    CRenderContext ctx;
    ASSERT_TRUE(ctx.begin());
    {
        CHLBufferReference aRef{a}, bRef{b};
        addSurfaceBufferUse(ctx.m_usedAsyncBuffers, first, aRef);
        addSurfaceBufferUse(ctx.m_usedAsyncBuffers, first, aRef);
        addSurfaceBufferUse(ctx.m_usedAsyncBuffers, second, aRef);
        addSurfaceBufferUse(ctx.m_usedAsyncBuffers, first, bRef);
    }
    EXPECT_EQ(ctx.m_usedAsyncBuffers.size(), 3U);
    EXPECT_EQ(a->m_locks, 2);
    EXPECT_EQ(b->m_locks, 1);
    EXPECT_EQ(a->m_releases, 0);
    auto uses = std::exchange(ctx.m_usedAsyncBuffers, {});
    ctx.reset();
    uses.clear();
    EXPECT_FALSE(a->locked());
    EXPECT_FALSE(b->locked());
    EXPECT_EQ(a->m_releases, 1);
    EXPECT_EQ(b->m_releases, 1);

    wl_display_flush_clients(m_display.get());
    std::array<uint32_t, 4> events{};
    ASSERT_EQ(recv(m_socket.get(), events.data(), sizeof(events), MSG_DONTWAIT), sc<ssize_t>(sizeof(events)));
    EXPECT_EQ(events[1], 8U << 16); // wl_buffer.release, opcode 0, no arguments
    EXPECT_EQ(events[3], 8U << 16);
}

TEST_F(CSurfaceBufferUseTest, ChildScopesKeepAllSampledBuffersThroughUnwind) {
    auto           a = buffer(), b = buffer();
    CRenderContext ctx;
    ASSERT_TRUE(ctx.begin());
    addSurfaceBufferUse(ctx.m_usedAsyncBuffers, {}, CHLBufferReference{a});
    try {
        auto outer = ctx.saveDrawState();
        auto inner = ctx.saveDrawState();
        addSurfaceBufferUse(ctx.m_usedAsyncBuffers, {}, CHLBufferReference{b});
        throw std::runtime_error("aborted immediate draw");
    } catch (const std::runtime_error&) {}

    EXPECT_EQ(ctx.m_usedAsyncBuffers.size(), 2U);
    EXPECT_EQ(a->m_releases, 0);
    EXPECT_EQ(b->m_releases, 0);
    std::vector<SSurfaceBufferUse> pending;
    mergeSurfaceBufferUses(pending, ctx.m_usedAsyncBuffers);
    ctx.reset();
    EXPECT_TRUE(a->locked());
    EXPECT_TRUE(b->locked());
    pending.clear(); // models the backend's eventual synchronization
    EXPECT_EQ(a->m_releases, 1);
    EXPECT_EQ(b->m_releases, 1);
}

TEST_F(CSurfaceBufferUseTest, ResetAndInactiveBeginRequireTransferredUses) {
    auto           a = buffer();
    CRenderContext ctx;
    addSurfaceBufferUse(ctx.m_usedAsyncBuffers, {}, CHLBufferReference{a});
    EXPECT_DEATH(ctx.reset(), "");
    EXPECT_DEATH((void)ctx.begin(), "");
    auto uses = std::exchange(ctx.m_usedAsyncBuffers, {});
    ASSERT_TRUE(ctx.begin());
    EXPECT_TRUE(ctx.m_usedAsyncBuffers.empty());
    EXPECT_TRUE(a->locked());
    ctx.reset();
    EXPECT_TRUE(a->locked());
    uses.clear();
    EXPECT_EQ(a->m_releases, 1);
}

TEST_F(CSurfaceBufferUseTest, FullFakeAndAbortedUsesMergeUntilNextSubmission) {
    auto                           a = buffer(), b = buffer();
    std::vector<SSurfaceBufferUse> pending;
    CRenderContext                 ctx;
    for (int i = 0; i < 64; ++i) {
        ASSERT_TRUE(ctx.begin());
        ctx.m_mode = RENDER_MODE_FULL_FAKE;
        addSurfaceBufferUse(ctx.m_usedAsyncBuffers, {}, CHLBufferReference{a});
        mergeSurfaceBufferUses(pending, ctx.m_usedAsyncBuffers);
        ctx.reset();
        ASSERT_EQ(pending.size(), 1U);
        EXPECT_EQ(a->m_locks, 1);
        EXPECT_EQ(a->m_releases, 0);
    }

    ASSERT_TRUE(ctx.begin());
    addSurfaceBufferUse(ctx.m_usedAsyncBuffers, {}, CHLBufferReference{b});
    mergeSurfaceBufferUses(pending, ctx.m_usedAsyncBuffers);
    ctx.reset();
    EXPECT_EQ(pending.size(), 2U);

    ASSERT_TRUE(ctx.begin());
    addSurfaceBufferUse(ctx.m_usedAsyncBuffers, {}, CHLBufferReference{a});
    mergeSurfaceBufferUses(pending, ctx.m_usedAsyncBuffers);
    auto completed = std::exchange(pending, {});
    ctx.reset();
    CFileDescriptor fence{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
    ASSERT_TRUE(fence.isValid());
    int callbacks = 0;
    releaseSurfaceBuffersOnReadable(fence.duplicate(), std::move(completed), [&] { ++callbacks; });
    EXPECT_TRUE(pending.empty());
    EXPECT_TRUE(a->locked());
    EXPECT_TRUE(b->locked());
    EXPECT_EQ(callbacks, 0);
    signal(fence);
    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(a->m_releases, 1);
    EXPECT_EQ(b->m_releases, 1);
}

TEST_F(CSurfaceBufferUseTest, TwoContextsHaveIndependentOutOfOrderCompletions) {
    auto           a = buffer(), b = buffer(), shared = buffer();
    CRenderContext first, second;
    ASSERT_TRUE(first.begin());
    ASSERT_TRUE(second.begin());
    addSurfaceBufferUse(first.m_usedAsyncBuffers, {}, CHLBufferReference{a});
    addSurfaceBufferUse(first.m_usedAsyncBuffers, {}, CHLBufferReference{shared});
    addSurfaceBufferUse(second.m_usedAsyncBuffers, {}, CHLBufferReference{b});
    addSurfaceBufferUse(second.m_usedAsyncBuffers, {}, CHLBufferReference{shared});
    auto firstUses  = std::exchange(first.m_usedAsyncBuffers, {});
    auto secondUses = std::exchange(second.m_usedAsyncBuffers, {});
    first.reset();
    second.reset();
    CFileDescriptor firstFence{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)}, secondFence{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
    ASSERT_TRUE(firstFence.isValid());
    ASSERT_TRUE(secondFence.isValid());
    releaseSurfaceBuffersOnReadable(firstFence.duplicate(), std::move(firstUses), {});
    releaseSurfaceBuffersOnReadable(secondFence.duplicate(), std::move(secondUses), {});
    signal(secondFence);
    EXPECT_TRUE(a->locked());
    EXPECT_FALSE(b->locked());
    EXPECT_TRUE(shared->locked());
    signal(firstFence);
    EXPECT_FALSE(a->locked());
    EXPECT_FALSE(shared->locked());
    EXPECT_EQ(shared->m_releases, 1);
}

TEST_F(CSurfaceBufferUseTest, DestroyedSurfaceRetainsPendingBufferUntilCompletion) {
    auto                           surf = surface();
    auto                           a    = buffer();
    CRenderContext                 ctx;
    std::vector<SSurfaceBufferUse> pending;
    ASSERT_TRUE(ctx.begin());
    ctx.m_mode = RENDER_MODE_FULL_FAKE;
    addSurfaceBufferUse(ctx.m_usedAsyncBuffers, surf, CHLBufferReference{a});
    mergeSurfaceBufferUses(pending, ctx.m_usedAsyncBuffers);
    ctx.reset();
    surf.reset();
    ASSERT_TRUE(pending.front().surface.expired());
    ASSERT_TRUE(a->m_resource->good());

    CFileDescriptor fence{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
    ASSERT_TRUE(fence.isValid());
    ASSERT_TRUE(attachSurfaceBufferReleaseFences(pending, fence));
    ASSERT_EQ(pending.size(), 1U);
    int callbacks = 0;
    releaseSurfaceBuffersOnReadable(fence.duplicate(), std::move(pending), [&] {
        EXPECT_FALSE(a->locked());
        ++callbacks;
    });
    dispatch();
    EXPECT_EQ(callbacks, 0);
    EXPECT_TRUE(a->locked());
    EXPECT_EQ(a->m_releases, 0);
    signal(fence);
    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(a->m_releases, 1);
}

TEST_F(CSurfaceBufferUseTest, DestroyedSurfaceStillAttachesReleaseFence) {
    auto                           surf = surface();
    auto                           a    = buffer();
    CHLBufferReference             producer{a};
    std::vector<SSurfaceBufferUse> uses;
    addSurfaceBufferUse(uses, surf, producer);
    surf.reset();
    ASSERT_TRUE(uses.front().surface.expired());

    std::array<int, 2> pipeFDs = {-1, -1};
    ASSERT_EQ(pipe2(pipeFDs.data(), O_CLOEXEC | O_NONBLOCK), 0);
    CFileDescriptor reader{pipeFDs[0]}, writer{pipeFDs[1]};
    // No DRM timeline is needed to observe the releaser's ownership of the FD.
    a->m_syncReleasers.emplace_back(makeUnique<CSyncReleaser>(nullptr, 1));
    ASSERT_TRUE(attachSurfaceBufferReleaseFences(uses, writer));
    EXPECT_TRUE(uses.empty());
    writer.reset();
    char byte = 0;
    EXPECT_EQ(read(reader.get(), &byte, 1), -1);
    EXPECT_EQ(errno, EAGAIN);
    producer = {};
    EXPECT_EQ(read(reader.get(), &byte, 1), 0);
    EXPECT_EQ(a->m_releases, 1);
}

TEST_F(CSurfaceBufferUseTest, FailedFenceMergePreservesPreviousFenceAndEveryBufferLock) {
    auto                           a = buffer(), b = buffer();
    std::vector<SSurfaceBufferUse> uses;
    addSurfaceBufferUse(uses, {}, CHLBufferReference{a});
    addSurfaceBufferUse(uses, {}, CHLBufferReference{b});
    a->m_syncReleasers.emplace_back(makeUnique<CSyncReleaser>(nullptr, 1));
    b->m_syncReleasers.emplace_back(makeUnique<CSyncReleaser>(nullptr, 2));

    std::array<int, 2> pipeFDs = {-1, -1};
    ASSERT_EQ(pipe2(pipeFDs.data(), O_CLOEXEC | O_NONBLOCK), 0);
    CFileDescriptor reader{pipeFDs[0]}, writer{pipeFDs[1]};
    ASSERT_TRUE(b->m_syncReleasers.front()->addSyncFileFd(writer));
    writer.reset();
    CFileDescriptor fence{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
    ASSERT_TRUE(fence.isValid());
    // Attaching to a succeeds, but a pipe cannot service b's SYNC_IOC_MERGE.
    EXPECT_FALSE(attachSurfaceBufferReleaseFences(uses, fence));
    EXPECT_EQ(uses.size(), 2U);
    EXPECT_TRUE(a->locked());
    EXPECT_TRUE(b->locked());
    EXPECT_EQ(a->m_releases, 0);
    EXPECT_EQ(b->m_releases, 0);
    char byte = 0;
    EXPECT_EQ(read(reader.get(), &byte, 1), -1);
    EXPECT_EQ(errno, EAGAIN);

    uses.clear(); // The GL caller synchronizes before releasing these locks.
    EXPECT_EQ(read(reader.get(), &byte, 1), 0);
    EXPECT_EQ(a->m_releases, 1);
    EXPECT_EQ(b->m_releases, 1);
}

TEST_F(CSurfaceBufferUseTest, InlineAndDeferredCallbacksCanBeginAnotherSession) {
    for (bool readable : {true, false}) {
        auto           oldBuffer = buffer(), newBuffer = buffer();
        CRenderContext ctx;
        ASSERT_TRUE(ctx.begin());
        addSurfaceBufferUse(ctx.m_usedAsyncBuffers, {}, CHLBufferReference{oldBuffer});
        auto uses = std::exchange(ctx.m_usedAsyncBuffers, {});
        ctx.reset();
        CFileDescriptor fence{eventfd(readable ? 1 : 0, EFD_CLOEXEC | EFD_NONBLOCK)};
        ASSERT_TRUE(fence.isValid());
        int callbacks = 0;
        releaseSurfaceBuffersOnReadable(fence.duplicate(), std::move(uses), [&] {
            ++callbacks;
            EXPECT_FALSE(oldBuffer->locked());
            ASSERT_TRUE(ctx.begin());
            EXPECT_TRUE(ctx.m_usedAsyncBuffers.empty());
            addSurfaceBufferUse(ctx.m_usedAsyncBuffers, {}, CHLBufferReference{newBuffer});
        });
        EXPECT_EQ(callbacks, readable ? 1 : 0);
        if (!readable)
            signal(fence);
        EXPECT_EQ(callbacks, 1);
        EXPECT_TRUE(ctx.active());
        EXPECT_EQ(ctx.m_usedAsyncBuffers.size(), 1U);
        EXPECT_TRUE(newBuffer->locked());
        auto nextUses = std::exchange(ctx.m_usedAsyncBuffers, {});
        ctx.reset();
        nextUses.clear();
        EXPECT_EQ(newBuffer->m_releases, 1);
    }
}

TEST_F(CSurfaceBufferUseTest, DiscardingWaiterDoesNotReportSuccessfulFrame) {
    auto                           a = buffer();
    std::vector<SSurfaceBufferUse> uses;
    addSurfaceBufferUse(uses, {}, CHLBufferReference{a});
    CFileDescriptor fence{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
    ASSERT_TRUE(fence.isValid());
    int callbacks = 0;
    releaseSurfaceBuffersOnReadable(std::move(fence), std::move(uses), [&] { ++callbacks; });
    EXPECT_TRUE(a->locked());
    // The GL backend finishes GPU work before event-loop shutdown discards waiters.
    g_pEventLoopManager.reset();
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(a->m_releases, 1);
}
