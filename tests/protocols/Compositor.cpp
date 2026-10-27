#include <Compositor.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <pointer/PointerManager.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/core/Compositor.hpp>
#include <protocols/core/Seat.hpp>
#include <protocols/core/Subcompositor.hpp>
#include <state/MonitorState.hpp>

#include <gtest/gtest.h>
#include <array>
#include <initializer_list>
#include <memory>
#include <sys/socket.h>

using namespace Hyprutils::OS;

class CCommitTestTexture : public Render::ITexture {
  public:
    void setTexParameter(GLenum, GLint) override {
        ADD_FAILURE();
    }
    void allocate(const Vector2D&, uint32_t) override {
        ADD_FAILURE();
    }
    void update(uint32_t, uint8_t*, uint32_t, const CRegion&) override {
        ADD_FAILURE();
    }
};

class CCommitTestBuffer : public IHLBuffer {
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
    void sendRelease() override {
        ++m_releases;
        IHLBuffer::sendRelease();
    }

    int m_releases = 0;
};

// Dispatch real surface requests on an isolated server, without a renderer or GPU.
class CSurfaceCommitTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        std::array<int, 2> sockets = {
            -1,
            -1,
        };
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        CFileDescriptor server{sockets[0]};
        m_socket = CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), server.get());
        ASSERT_NE(m_client, nullptr);
        server.take();

        m_previousCompositor   = std::move(g_pCompositor);
        m_previousEventLoop    = std::move(g_pEventLoopManager);
        m_previousMonitorState = std::move(State::monitorState());
        m_previousEventBus     = std::move(Event::bus());
        m_previousPresentation = std::move(PROTO::presentation);
        m_installedGlobals     = true;

        Event::bus()               = makeUnique<Event::CEventBus>();
        State::monitorState()      = makeUnique<State::CMonitorStateTracker>();
        g_pCompositor              = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay = m_display.get();
        g_pEventLoopManager        = makeUnique<CEventLoopManager>(m_display.get(), wl_display_get_event_loop(m_display.get()));
        PROTO::presentation        = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "surface-commit-test");
    }

    void TearDown() override {
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        if (!m_installedGlobals)
            return;

        PROTO::presentation   = std::move(m_previousPresentation);
        g_pEventLoopManager   = std::move(m_previousEventLoop);
        g_pCompositor         = std::move(m_previousCompositor);
        State::monitorState() = std::move(m_previousMonitorState);
        Event::bus()          = std::move(m_previousEventBus);
    }

    SP<CWLSurfaceResource> surface() {
        auto surface          = makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(m_client, 6, m_nextID++));
        surface->m_self       = surface;
        surface->m_stateQueue = CSurfaceStateQueue{surface};
        return surface;
    }

    SP<CCommitTestBuffer> buffer() {
        auto buffer                  = makeShared<CCommitTestBuffer>();
        auto wire                    = makeShared<CWlBuffer>(m_client, 1, m_nextID++);
        buffer->m_resource           = CWLBufferResource::create(wire);
        buffer->m_resource->m_buffer = buffer;
        buffer->size                 = {16, 16};
        buffer->m_texture            = makeShared<CCommitTestTexture>();
        return buffer;
    }

    std::pair<SP<CWLSubsurfaceResource>, uint32_t> subsurface(SP<CWLSurfaceResource> child, SP<CWLSurfaceResource> parent) {
        const auto id         = m_nextID++;
        auto       subsurface = makeShared<CWLSubsurfaceResource>(makeShared<CWlSubsurface>(m_client, 1, id), child, parent);
        subsurface->m_self    = subsurface;
        child->m_role         = makeShared<CSubsurfaceRole>(subsurface);
        parent->m_subsurfaces.emplace_back(subsurface);
        request(id, 4); // wl_subsurface.set_sync
        return {
            subsurface,
            id,
        };
    }

    void request(uint32_t id, uint32_t opcode, std::initializer_list<uint32_t> args = {}) {
        std::vector<uint32_t> message = {
            id,
            sc<uint32_t>((args.size() + 2) * sizeof(uint32_t) << 16) | opcode,
        };
        message.insert(message.end(), args);
        const auto size = message.size() * sizeof(uint32_t);
        ASSERT_EQ(send(m_socket.get(), message.data(), size, MSG_NOSIGNAL), sc<ssize_t>(size));
        ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    }

    void commit(SP<CWLSurfaceResource> surface) {
        request(surface->id(), 6); // wl_surface.commit
    }

    void attach(SP<CWLSurfaceResource> surface, SP<CCommitTestBuffer> buffer = nullptr) {
        request(surface->id(), 1, {buffer ? wl_resource_get_id(buffer->m_resource->getResource()) : 0, 0, 0}); // wl_surface.attach
        commit(surface);
    }

    wl_client* m_client = nullptr; // Owned by m_display.
    uint32_t   m_nextID = 2;

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    bool                                                       m_installedGlobals = false;
    UP<CCompositor>                                            m_previousCompositor;
    UP<CEventLoopManager>                                      m_previousEventLoop;
    UP<State::CMonitorStateTracker>                            m_previousMonitorState;
    UP<Event::CEventBus>                                       m_previousEventBus;
    UP<CPresentationProtocol>                                  m_previousPresentation;
};

TEST_F(CSurfaceCommitTest, NullBufferAfterRoleRemovalAllowsEmptyInitialCommit) {
    auto surf = surface();
    attach(surf, buffer());
    surf->map(); // The previous role left its wl_surface mapped.

    int  commits  = 0;
    auto listener = surf->m_events.commit.listen([&] { ++commits; });
    attach(surf);
    EXPECT_FALSE(surf->m_mapped);
    EXPECT_EQ(commits, 1);

    // A recreated role must receive its initial commit even with no new surface state.
    commit(surf);
    EXPECT_EQ(commits, 2);
}

TEST_F(CSurfaceCommitTest, SynchronizedNullBufferUnmapsOnlyOnParentCommit) {
    auto parent = surface(), child = surface();
    auto [sub, id] = subsurface(child, parent);
    attach(parent, buffer());
    parent->map();
    attach(child, buffer());
    commit(parent);
    ASSERT_TRUE(child->m_mapped);

    int  unmaps   = 0;
    auto listener = child->m_events.unmap.listen([&] { ++unmaps; });
    attach(child);
    EXPECT_TRUE(child->m_mapped);
    EXPECT_EQ(unmaps, 0);

    commit(parent);
    EXPECT_FALSE(child->m_mapped);
    EXPECT_EQ(unmaps, 1);
    commit(parent);
    EXPECT_EQ(unmaps, 1);
}

TEST_F(CSurfaceCommitTest, CursorReusedAfterNullBufferReleasesOnDeselection) {
    auto surf      = surface();
    surf->m_role   = makeShared<CCursorSurfaceRole>();
    auto wlSurface = Desktop::View::CWLSurface::create();
    wlSurface->assign(surf);
    Pointer::CPointerManager pointer;
    pointer.setCursorSurface(wlSurface, {});
    attach(surf, buffer());

    attach(surf);
    EXPECT_TRUE(surf->m_mapped);
    auto replacement = buffer();
    attach(surf, replacement);
    pointer.setCursorSurface(wlSurface, {});
    EXPECT_TRUE(surf->m_mapped);
    ASSERT_TRUE(replacement->locked());

    pointer.setCursorSurface(nullptr, {});
    EXPECT_FALSE(surf->m_mapped);
    EXPECT_FALSE(replacement->locked());
    EXPECT_EQ(replacement->m_releases, 1);
}
