#include <Compositor.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <pointer/PointerManager.hpp>
#include <protocols/InputMethodV2.hpp>
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

        m_previousCompositor     = std::move(g_pCompositor);
        m_previousEventLoop      = std::move(g_pEventLoopManager);
        m_previousMonitorState   = std::move(State::monitorState());
        m_previousEventBus       = std::move(Event::bus());
        m_previousPresentation   = std::move(PROTO::presentation);
        m_previousCoreCompositor = std::move(PROTO::compositor);
        m_previousSubcompositor  = std::move(PROTO::subcompositor);
        m_installedGlobals       = true;

        Event::bus()               = makeUnique<Event::CEventBus>();
        State::monitorState()      = makeUnique<State::CMonitorStateTracker>();
        g_pCompositor              = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay = m_display.get();
        g_pEventLoopManager        = makeUnique<CEventLoopManager>(m_display.get(), wl_display_get_event_loop(m_display.get()));
        PROTO::presentation        = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "surface-commit-test");
        PROTO::compositor          = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "surface-commit-test-compositor");
        PROTO::subcompositor       = makeUnique<CWLSubcompositorProtocol>(&wl_subcompositor_interface, 1, "surface-commit-test-subcompositor");
    }

    void TearDown() override {
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        if (!m_installedGlobals)
            return;

        PROTO::subcompositor  = std::move(m_previousSubcompositor);
        PROTO::compositor     = std::move(m_previousCoreCompositor);
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

    void expectProtocolError(uint32_t objectID, uint32_t code) {
        // The rejected request disconnects the client and flushes wl_display.error.
        std::array<uint32_t, 4> error = {};
        ASSERT_EQ(recv(m_socket.get(), error.data(), sizeof(error), MSG_DONTWAIT), sc<ssize_t>(sizeof(error)));
        EXPECT_EQ(error[0], 1U);           // wl_display
        EXPECT_EQ(error[1] & 0xFFFFU, 0U); // error
        EXPECT_EQ(error[2], objectID);
        EXPECT_EQ(error[3], code);
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
    UP<CWLCompositorProtocol>                                  m_previousCoreCompositor;
    UP<CWLSubcompositorProtocol>                               m_previousSubcompositor;
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

TEST_F(CSurfaceCommitTest, SubsurfaceAncestryTraversesEveryParent) {
    auto root = surface(), ancestor = surface(), parent = surface(), child = surface();
    auto [ancestorSub, ancestorID] = subsurface(ancestor, root);
    auto [parentSub, parentID]     = subsurface(parent, ancestor);
    auto [childSub, childID]       = subsurface(child, parent);
    request(ancestorID, 1, {100, 200}); // wl_subsurface.set_position
    request(parentID, 1, {10, 20});
    request(childID, 1, {1, 2});

    EXPECT_EQ(childSub->posRelativeToParent(), Vector2D(111, 222));
    EXPECT_EQ(childSub->t1Parent(), root);
}

TEST_F(CSurfaceCommitTest, SubsurfaceAncestryStopsAtCycle) {
    auto first = surface(), second = surface(), child = surface();
    auto [firstSub, firstID]   = subsurface(first, second);
    auto [secondSub, secondID] = subsurface(second, first);
    auto [childSub, childID]   = subsurface(child, first);
    request(firstID, 1, {100, 200}); // wl_subsurface.set_position
    request(secondID, 1, {10, 20});
    request(childID, 1, {1, 2});

    EXPECT_EQ(childSub->posRelativeToParent(), Vector2D(111, 222));
    EXPECT_EQ(childSub->t1Parent(), first);
}

TEST_F(CSurfaceCommitTest, SubsurfaceAncestryHandlesExpiredParentDuringChildDestruction) {
    auto parent = surface(), child = surface();
    auto [sub, id] = subsurface(child, parent);
    request(id, 1, {7, 13}); // wl_subsurface.set_position
    parent.reset();
    ASSERT_TRUE(sub->m_parent.expired());

    int  destroys = 0;
    auto listener = child->m_events.destroy.listen([&] {
        ++destroys;
        EXPECT_EQ(sub->posRelativeToParent(), Vector2D(7, 13));
        EXPECT_FALSE(sub->t1Parent());
    });
    child.reset();
    EXPECT_EQ(destroys, 1);
}

TEST_F(CSurfaceCommitTest, SubsurfaceAncestryHandlesExpiredIntermediateAncestor) {
    auto root = surface(), ancestor = surface(), parent = surface(), child = surface();
    auto [ancestorSub, ancestorID] = subsurface(ancestor, root);
    auto [parentSub, parentID]     = subsurface(parent, ancestor);
    auto [childSub, childID]       = subsurface(child, parent);
    request(ancestorID, 1, {100, 200}); // wl_subsurface.set_position
    request(parentID, 1, {10, 20});
    request(childID, 1, {1, 2});
    ancestor.reset();
    ASSERT_TRUE(childSub->m_parent.lock());
    ASSERT_TRUE(parentSub->m_parent.expired());

    EXPECT_EQ(childSub->posRelativeToParent(), Vector2D(11, 22));
    EXPECT_FALSE(childSub->t1Parent());
}

TEST_F(CSurfaceCommitTest, SubsurfaceAncestryHandlesDestroyingParentRole) {
    auto root = surface(), parent = surface(), child = surface();
    auto [parentSub, parentID] = subsurface(parent, root);
    auto [childSub, childID]   = subsurface(child, parent);
    request(parentID, 1, {10, 20}); // wl_subsurface.set_position
    request(childID, 1, {1, 2});

    int  destroys = 0;
    auto listener = parentSub->m_events.destroy.listen([&] {
        ++destroys;
        ASSERT_EQ(parent->m_role->role(), SURFACE_ROLE_SUBSURFACE);
        ASSERT_TRUE(sc<CSubsurfaceRole*>(parent->m_role.get())->m_subsurface.expired());
        EXPECT_EQ(childSub->posRelativeToParent(), Vector2D(1, 2));
        EXPECT_FALSE(childSub->t1Parent());
    });
    parentSub.reset();
    EXPECT_EQ(destroys, 1);
}

TEST_F(CSurfaceCommitTest, GetSubsurfaceRejectsExpiredParentRole) {
    auto parent = surface(), child = surface();
    // Model the expired role reference visible during subsurface destruction.
    parent->m_role       = makeShared<CSubsurfaceRole>(nullptr);
    const auto managerID = m_nextID++;
    PROTO::subcompositor->bindManager(m_client, nullptr, 1, managerID);
    ASSERT_NE(wl_client_get_object(m_client, managerID), nullptr);

    request(managerID, 1, {m_nextID++, child->id(), parent->id()}); // wl_subcompositor.get_subsurface
    expectProtocolError(managerID, WL_SUBCOMPOSITOR_ERROR_BAD_PARENT);
    EXPECT_EQ(child->m_role->role(), SURFACE_ROLE_UNASSIGNED);
}

class CSubsurfaceImageDescriptionTest : public CSurfaceCommitTest {
  protected:
    void SetUp() override {
        CSurfaceCommitTest::SetUp();
        if (HasFatalFailure())
            return;

        m_previousConfig   = std::move(Config::mgr());
        m_previousWatcher  = std::move(Config::watcher());
        m_installedGlobals = true;

        // Construct defaults only; do not load the user's config.
        g_pCompositor->m_explicitConfigPath = "subsurface-image-description-test.lua";
        Config::Supplementary::Jeremy::flushCachedCfgPath();
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
    }

    void TearDown() override {
        if (m_installedGlobals) {
            Config::mgr()     = std::move(m_previousConfig);
            Config::watcher() = std::move(m_previousWatcher);
            if (Config::mgr())
                CConfigValueBase::flushCaches();
        }
        CSurfaceCommitTest::TearDown();
        Config::Supplementary::Jeremy::flushCachedCfgPath();
    }

  private:
    bool                       m_installedGlobals = false;
    UP<Config::IConfigManager> m_previousConfig;
    UP<Config::CConfigWatcher> m_previousWatcher;
};

TEST_F(CSubsurfaceImageDescriptionTest, OrphanedSubsurfaceUsesFallback) {
    auto parent = surface(), child = surface();
    auto [sub, id] = subsurface(child, parent);
    parent.reset();
    ASSERT_TRUE(sub->m_parent.expired());

    EXPECT_EQ(child->getPreferredImageDescription(), g_pCompositor->getPreferredImageDescription());
}

TEST_F(CSubsurfaceImageDescriptionTest, DestroyingSubsurfaceRoleUsesFallback) {
    auto parent = surface(), child = surface();
    auto [sub, id] = subsurface(child, parent);

    int  destroys = 0;
    auto listener = sub->m_events.destroy.listen([&] {
        ++destroys;
        ASSERT_EQ(child->m_role->role(), SURFACE_ROLE_SUBSURFACE);
        ASSERT_TRUE(sc<CSubsurfaceRole*>(child->m_role.get())->m_subsurface.expired());
        EXPECT_EQ(child->getPreferredImageDescription(), g_pCompositor->getPreferredImageDescription());
    });
    sub.reset();
    EXPECT_EQ(destroys, 1);
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

class CSubsurfacePlacementTest : public CSurfaceCommitTest, public ::testing::WithParamInterface<uint32_t> {};

TEST_P(CSubsurfacePlacementTest, ParentOnlyCommitNotifiesPlacementObservers) {
    auto parent = surface(), child = surface();
    auto [sub, id] = subsurface(child, parent);
    attach(parent, buffer());
    parent->map();
    if (GetParam() == 2)
        request(id, 3, {parent->id()}); // Start below the parent for place_above.
    attach(child, buffer());
    commit(parent);
    ASSERT_TRUE(child->m_mapped);

    int  parentCommits = 0, childCommits = 0;
    auto parentListener = parent->m_events.commit.listen([&] { ++parentCommits; });
    auto childListener  = child->m_events.commit.listen([&] { ++childCommits; });
    commit(parent);
    EXPECT_EQ(parentCommits, 0);
    EXPECT_EQ(childCommits, 0);

    if (GetParam() == 1)
        request(id, 1, {12, 24}); // set_position
    else
        request(id, GetParam(), {parent->id()}); // place_above / place_below
    EXPECT_EQ(parentCommits, 0);
    EXPECT_EQ(childCommits, 0);

    // Placement is parent-buffered; no child commit should be necessary.
    commit(parent);
    EXPECT_EQ(parentCommits, 1);
    EXPECT_EQ(childCommits, 1);
    commit(parent);
    EXPECT_EQ(parentCommits, 1);
    EXPECT_EQ(childCommits, 1);
}

INSTANTIATE_TEST_SUITE_P(PlacementRequests, CSubsurfacePlacementTest, ::testing::Values(1U, 2U, 3U));

TEST_F(CSurfaceCommitTest, PlacementOnSynchronizedParentPropagatesAfterParentCommit) {
    auto root = surface(), parent = surface(), child = surface();
    auto [parentSub, parentID] = subsurface(parent, root);
    auto [childSub, childID]   = subsurface(child, parent);
    attach(root, buffer());
    root->map();
    attach(parent, buffer());
    attach(child, buffer());
    commit(root);
    ASSERT_TRUE(parent->m_mapped);
    ASSERT_TRUE(child->m_mapped);

    int  parentCommits = 0, childCommits = 0;
    auto parentListener = parent->m_events.commit.listen([&] { ++parentCommits; });
    auto childListener  = child->m_events.commit.listen([&] { ++childCommits; });
    request(childID, 1, {12, 24});
    commit(root);
    EXPECT_EQ(parentCommits, 0);
    EXPECT_EQ(childCommits, 0);

    commit(parent);
    EXPECT_EQ(parentCommits, 0);
    EXPECT_EQ(childCommits, 0);
    commit(root);
    EXPECT_EQ(parentCommits, 1);
    EXPECT_EQ(childCommits, 1);
}

TEST_F(CSurfaceCommitTest, InputRectangleDoesNotRearmEmptyPopupCommits) {
    auto                surf = surface();
    CInputMethodPopupV2 popup{makeShared<CZwpInputPopupSurfaceV2>(m_client, 1, m_nextID++), nullptr, surf};
    attach(surf, buffer());
    ASSERT_TRUE(popup.m_mapped);

    int  commits  = 0;
    auto listener = popup.m_events.commit.listen([&] {
        ++commits;
        // CInputPopup::onCommit sends the current rectangle back to the client.
        popup.sendInputRectangle({0, 0, 16, 16});
    });

    popup.sendInputRectangle({0, 0, 16, 16});
    commit(surf);
    commit(surf);
    EXPECT_EQ(commits, 0);

    request(surf->id(), 2, {0, 0, 16, 16}); // wl_surface.damage
    commit(surf);
    EXPECT_EQ(commits, 1);
    commit(surf);
    EXPECT_EQ(commits, 1);
}
