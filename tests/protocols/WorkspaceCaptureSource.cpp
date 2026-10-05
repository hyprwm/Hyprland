#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <desktop/state/WindowState.hpp>
#include <event/EventBus.hpp>
#include <ipc/s2/S2.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <managers/screenshare/WorkspaceCaptureSource.hpp>
#include <output/Monitor.hpp>
#include <protocols/ExtWorkspace.hpp>
#include <protocols/ImageCaptureSource.hpp>
#include <render/Renderer.hpp>
#include <workspace/HLWorkspace.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <functional>
#include <memory>
#include <string_view>
#include <sys/socket.h>

using namespace Hyprutils::OS;

// Skip CHLWorkspace::init: these tests drive its public lifecycle signals without
// creating a layout, running workspace rules, or publishing compositor events.
class CCaptureTestWorkspace : public Workspace::CHLWorkspace {
  public:
    explicit CCaptureTestWorkspace(PHLMONITOR monitor) : CHLWorkspace(Workspace::SWorkspaceNumberedID{73}, monitor, "Capture workspace", "73", Workspace::eWorkspaceType::NORMAL) {
        ;
    }

    const std::string& addressableName() const override {
        return m_testName;
    }

    void renameForCapture(const std::string& name) {
        m_testName = name;
        m_events.renamed.emit();
    }

  private:
    std::string m_testName = "73";
};

class CCaptureTestOutput : public Aquamarine::IOutput {
  public:
    bool commit() override {
        ADD_FAILURE() << "Unexpected output commit";
        return false;
    }
    bool test() override {
        ADD_FAILURE() << "Unexpected output test";
        return false;
    }
    SP<Aquamarine::IBackendImplementation> getBackend() override {
        return nullptr;
    }
    std::vector<SDRMFormat> getRenderFormats() override {
        return {};
    }
    bool pendingPageFlip() override {
        return false;
    }
    bool pendingIdleFrame() override {
        return false;
    }
};

class CWorkspaceCaptureSourceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_NE(m_display, nullptr);

        m_previousCompositor = std::move(g_pCompositor);
        m_previousEventLoop  = std::move(g_pEventLoopManager);
        m_previousRenderer   = std::move(g_pHyprRenderer);
        m_previousWindows    = std::move(Desktop::windowState());
        m_previousEventBus   = std::move(Event::bus());
        m_previousWatcher    = std::move(Config::watcher());
        m_previousConfig     = std::move(Config::mgr());
        m_previousAnimations = std::move(Animation::mgr());
        m_previousTree       = std::move(Config::animationTree());
        m_installedGlobals   = true;

        Event::bus()           = makeUnique<Event::CEventBus>();
        Desktop::windowState() = makeUnique<Desktop::CWindowState>();
        // As in DMABUF.cpp, config-only construction never starts a session.
        // Monitor metadata needs animations/config, but no renderer or backend.
        g_pCompositor                = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay   = m_display.get();
        g_pCompositor->m_wlEventLoop = wl_display_get_event_loop(m_display.get());
        // Keep workspace destruction from binding a real IPC socket.
        g_pCompositor->m_instancePath = std::string(128, 'x');
        m_previousSocket2             = std::move(IPC::Socket2::sock());
        IPC::Socket2::sock()          = makeUnique<IPC::Socket2::CSocket2>();
        g_pEventLoopManager           = makeUnique<CEventLoopManager>(m_display.get(), g_pCompositor->m_wlEventLoop);
        Config::watcher()             = makeUnique<Config::CConfigWatcher>();
        Config::mgr()                 = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
        Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree() = makeUnique<Config::CAnimationTreeController>();

        m_monitor              = makeMonitor({1080, 1920});
        m_monitor->m_position  = {100, 200};
        m_monitor->m_size      = {540, 960};
        m_monitor->m_pixelSize = {1920, 1080};
        m_monitor->m_scale     = 2.F;
        m_monitor->m_transform = WL_OUTPUT_TRANSFORM_90;
        m_workspace            = makeShared<CCaptureTestWorkspace>(m_monitor);
        m_workspace->m_self    = m_workspace;
    }

    void TearDown() override {
        if (!m_installedGlobals)
            return;

        m_workspace.reset();
        m_monitor.reset();
        IPC::Socket2::sock()    = std::move(m_previousSocket2);
        Animation::mgr()        = std::move(m_previousAnimations);
        Config::animationTree() = std::move(m_previousTree);
        Config::mgr()           = std::move(m_previousConfig);
        Config::watcher()       = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pEventLoopManager    = std::move(m_previousEventLoop);
        g_pCompositor          = std::move(m_previousCompositor);
        g_pHyprRenderer        = std::move(m_previousRenderer);
        Desktop::windowState() = std::move(m_previousWindows);
        Event::bus()           = std::move(m_previousEventBus);
    }

    PHLMONITOR makeMonitor(const Vector2D& size) {
        auto output     = makeShared<CCaptureTestOutput>();
        output->name    = "workspace-capture-test";
        auto monitor    = makeShared<Monitor::CMonitor>(output);
        monitor->m_self = monitor;
        monitor->m_size = monitor->m_pixelSize = monitor->m_transformedSize = size;
        return monitor;
    }

    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    PHLMONITOR                                                 m_monitor;
    SP<CCaptureTestWorkspace>                                  m_workspace;

  private:
    bool                                 m_installedGlobals = false;
    UP<CCompositor>                      m_previousCompositor;
    UP<CEventLoopManager>                m_previousEventLoop;
    UP<Render::IHyprRenderer>            m_previousRenderer;
    UP<IPC::Socket2::CSocket2>           m_previousSocket2;
    UP<Desktop::CWindowState>            m_previousWindows;
    UP<Event::CEventBus>                 m_previousEventBus;
    UP<Config::CConfigWatcher>           m_previousWatcher;
    UP<Config::IConfigManager>           m_previousConfig;
    UP<Animation::CHyprAnimationManager> m_previousAnimations;
    UP<Config::CAnimationTreeController> m_previousTree;
};

TEST_F(CWorkspaceCaptureSourceTest, UsesTransformedPixelsAndPreservesBothModes) {
    for (const auto mode :
         {HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_WINDOWS_ONLY, HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING}) {
        SCOPED_TRACE(mode);
        Screenshare::CWorkspaceCaptureSource source{m_workspace, mode};
        EXPECT_EQ(source.monitor(), m_monitor);
        EXPECT_EQ(source.bufferSize(), Vector2D(1080, 1920));
        EXPECT_EQ(source.name(), "73");
        EXPECT_NE(source.name(), m_workspace->displayName());
        EXPECT_FALSE(source.removed());
        EXPECT_EQ(source.mode(), sc<uint32_t>(mode));
    }
}

TEST_F(CWorkspaceCaptureSourceTest, MoveReplacesModeListenerAndIgnoresInvalidSizes) {
    Screenshare::CWorkspaceCaptureSource source{m_workspace, 0};
    int                                  changes  = 0;
    auto                                 listener = source.m_changed.listen([&] { ++changes; });

    auto                                 next = makeMonitor({2560, 1440});
    m_workspace->m_monitor                    = next;
    m_workspace->m_events.monitorChanged.emit();
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(source.monitor(), next);
    EXPECT_EQ(source.bufferSize(), Vector2D(2560, 1440));

    m_monitor->m_transformedSize = {800, 600};
    m_monitor->m_events.modeChanged.emit();
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(source.bufferSize(), Vector2D(2560, 1440));

    next->m_transformedSize = {3840, 2160};
    next->m_events.modeChanged.emit();
    EXPECT_EQ(changes, 2);
    EXPECT_EQ(source.bufferSize(), Vector2D(3840, 2160));

    for (const auto size : {Vector2D{0, 2160}, Vector2D{3840, 0}, Vector2D{-1, 2160}, Vector2D{3840, -1}}) {
        next->m_transformedSize = size;
        next->m_events.modeChanged.emit();
        EXPECT_EQ(source.bufferSize(), Vector2D(3840, 2160));
    }
    EXPECT_EQ(changes, 6);

    m_workspace->m_monitor.reset();
    m_workspace->m_events.monitorChanged.emit();
    EXPECT_FALSE(source.monitor());
    EXPECT_EQ(source.bufferSize(), Vector2D(3840, 2160));
    next->m_events.modeChanged.emit();
    EXPECT_EQ(changes, 7);

    m_workspace->m_monitor = m_monitor;
    m_workspace->m_events.monitorChanged.emit();
    EXPECT_EQ(source.bufferSize(), Vector2D(800, 600));
    m_monitor->m_transformedSize = {1280, 720};
    m_monitor->m_events.modeChanged.emit();
    EXPECT_EQ(source.bufferSize(), Vector2D(1280, 720));
    EXPECT_EQ(changes, 9);
}

TEST_F(CWorkspaceCaptureSourceTest, RenameAndRemovalFreezeStateAndDetachListeners) {
    Screenshare::CWorkspaceCaptureSource source{m_workspace, HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING};
    int                                  changes  = 0;
    auto                                 listener = source.m_changed.listen([&] { ++changes; });

    m_workspace->renameForCapture("renamed");
    EXPECT_EQ(source.name(), "renamed");
    EXPECT_EQ(changes, 1);
    m_workspace->m_events.destroy.emit();
    EXPECT_EQ(changes, 2);
    EXPECT_TRUE(source.removed());
    EXPECT_EQ(source.monitor(), m_monitor);

    m_monitor->m_transformedSize = {800, 600};
    m_monitor->m_events.modeChanged.emit();
    m_workspace->renameForCapture("too late");
    auto other             = makeMonitor({640, 480});
    m_workspace->m_monitor = other;
    m_workspace->m_events.monitorChanged.emit();
    m_workspace->m_events.destroy.emit();
    EXPECT_EQ(changes, 2);
    EXPECT_EQ(source.monitor(), m_monitor);
    EXPECT_EQ(source.bufferSize(), Vector2D(1080, 1920));
    EXPECT_EQ(source.name(), "renamed");
    EXPECT_EQ(source.mode(), sc<uint32_t>(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING));
}

TEST_F(CWorkspaceCaptureSourceTest, SurvivesWorkspaceDestructionWithoutOwningWorkspaceOrMonitor) {
    auto            source    = makeShared<Screenshare::CWorkspaceCaptureSource>(m_workspace, 0);
    PHLWORKSPACEREF workspace = m_workspace;
    PHLMONITORREF   monitor   = m_monitor;
    int             changes   = 0;
    auto            listener  = source->m_changed.listen([&] { ++changes; });

    m_workspace.reset();
    EXPECT_TRUE(workspace.expired());
    EXPECT_TRUE(source->removed());
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(source->monitor(), m_monitor);
    m_monitor.reset();
    EXPECT_TRUE(monitor.expired());
    EXPECT_FALSE(source->monitor());
    EXPECT_EQ(source->bufferSize(), Vector2D(1080, 1920));
    EXPECT_EQ(source->name(), "73");
    EXPECT_EQ(changes, 1);
}

TEST_F(CWorkspaceCaptureSourceTest, NullWorkspaceAndInitiallyMissingMonitor) {
    Screenshare::CWorkspaceCaptureSource removed{nullptr, 1};
    EXPECT_TRUE(removed.removed());
    EXPECT_FALSE(removed.monitor());
    EXPECT_EQ(removed.bufferSize(), Vector2D(0, 0));
    EXPECT_TRUE(removed.name().empty());
    EXPECT_EQ(removed.mode(), 1U);

    m_workspace->m_monitor.reset();
    Screenshare::CWorkspaceCaptureSource source{m_workspace, 0};
    EXPECT_FALSE(source.removed());
    EXPECT_FALSE(source.monitor());
    EXPECT_EQ(source.bufferSize(), Vector2D(0, 0));
    m_workspace->m_monitor = m_monitor;
    m_workspace->m_events.monitorChanged.emit();
    EXPECT_EQ(source.monitor(), m_monitor);
    EXPECT_EQ(source.bufferSize(), Vector2D(1080, 1920));
}

// Exercise generated request dispatch over a private socketpair. Only the
// ext-workspace handle is seeded directly through its public manager API.
class CWorkspaceCaptureProtocolTest : public CWorkspaceCaptureSourceTest {
  protected:
    void SetUp() override {
        CWorkspaceCaptureSourceTest::SetUp();
        if (HasFatalFailure())
            return;

        m_previousSources         = std::move(PROTO::imageCaptureSource);
        m_previousWorkspaces      = std::move(PROTO::extWorkspace);
        m_installedProtocols      = true;
        PROTO::imageCaptureSource = makeUnique<CImageCaptureSourceProtocol>();
        PROTO::extWorkspace       = makeUnique<CExtWorkspaceProtocol>(&ext_workspace_manager_v1_interface, 1, "workspace-capture-test");

        std::array<int, 2> sockets = {-1, -1};
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        CFileDescriptor serverSocket{sockets[0]};
        m_socket = CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), serverSocket.get());
        ASSERT_NE(m_client, nullptr);
        serverSocket.take();

        // wl_display.get_registry(new_id=2), then bind the global installed by
        // CImageCaptureSourceProtocol's constructor rather than a second global.
        request({1, (12U << 16) | 1U, 2});
        uint32_t global = 0;
        receiveEvents([&](const auto& event) {
            if (event[0] == 2 && (event[1] & 0xFFFF) == 0 && event.size() >= 5 &&
                std::string_view(rc<const char*>(event.data() + 4), event[3] - 1) == hyprland_workspace_image_capture_source_manager_v1_interface.name)
                global = event[2];
        });
        ASSERT_NE(global, 0U);
        const std::string     interface   = hyprland_workspace_image_capture_source_manager_v1_interface.name;
        const size_t          stringWords = (interface.size() + 1 + 3) / 4;
        std::vector<uint32_t> bind(6 + stringWords, 0);
        bind[0] = 2;
        bind[1] = sc<uint32_t>(bind.size() * sizeof(uint32_t)) << 16;
        bind[2] = global;
        bind[3] = interface.size() + 1;
        std::memcpy(bind.data() + 4, interface.c_str(), interface.size() + 1);
        bind[4 + stringWords] = 1;
        bind[5 + stringWords] = 3;
        request(bind);
        ASSERT_NE(wl_client_get_object(m_client, 3), nullptr);

        m_workspaceManager         = makeShared<CExtWorkspaceManagerResource>(makeUnique<CExtWorkspaceManagerV1>(m_client, 1, 4));
        m_workspaceManager->m_self = m_workspaceManager;
        m_workspaceManager->onWorkspaceCreated(m_workspace);
        receiveEvents([&](const auto& event) {
            if (event[0] == 4 && (event[1] & 0xFFFF) == 1 && event.size() == 3)
                m_handle = event[2];
        });
        ASSERT_NE(m_handle, 0U);
        ASSERT_EQ(PROTO::extWorkspace->workspaceFromHandle(wl_client_get_object(m_client, m_handle)), m_workspace);
    }

    void TearDown() override {
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        m_workspaceManager.reset();
        if (m_installedProtocols) {
            PROTO::imageCaptureSource = std::move(m_previousSources);
            PROTO::extWorkspace       = std::move(m_previousWorkspaces);
        }
        CWorkspaceCaptureSourceTest::TearDown();
    }

    void request(const std::vector<uint32_t>& words) {
        ASSERT_EQ(send(m_socket.get(), words.data(), words.size() * sizeof(uint32_t), MSG_NOSIGNAL), sc<ssize_t>(words.size() * sizeof(uint32_t)));
        ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    }

    void receiveEvents(const std::function<void(const std::vector<uint32_t>&)>& callback) {
        wl_display_flush_clients(m_display.get());
        // The isolated server has already dispatched and flushed these small
        // messages. Nonblocking reads keep failures from hanging the test runner.
        for (;;) {
            std::array<uint32_t, 2> header = {};
            const auto              bytes  = recv(m_socket.get(), header.data(), sizeof(header), MSG_DONTWAIT);
            if (bytes == 0 || (bytes < 0 && errno == EAGAIN))
                return;
            ASSERT_EQ(bytes, sc<ssize_t>(sizeof(header)));
            const auto size = header[1] >> 16;
            ASSERT_GE(size, sizeof(header));
            ASSERT_EQ(size % sizeof(uint32_t), 0U);
            std::vector<uint32_t> event(size / sizeof(uint32_t));
            event[0]               = header[0];
            event[1]               = header[1];
            const auto payloadSize = size - sizeof(header);
            const auto received    = payloadSize ? recv(m_socket.get(), event.data() + 2, payloadSize, MSG_DONTWAIT) : 0;
            ASSERT_EQ(received, sc<ssize_t>(payloadSize));
            callback(event);
        }
    }

    void createSource(uint32_t id, uint32_t mode) {
        request({3, 20U << 16, id, m_handle, mode});
    }

    SP<CImageCaptureSource> source(uint32_t id) {
        auto resource = wl_client_get_object(m_client, id);
        return resource ? PROTO::imageCaptureSource->sourceFromResource(resource) : nullptr;
    }

    CFileDescriptor m_socket;
    wl_client*      m_client = nullptr; // Owned by m_display; invalid after disconnect/error.
    uint32_t        m_handle = 0;

  private:
    bool                             m_installedProtocols = false;
    SP<CExtWorkspaceManagerResource> m_workspaceManager;
    UP<CImageCaptureSourceProtocol>  m_previousSources;
    UP<CExtWorkspaceProtocol>        m_previousWorkspaces;
};

TEST_F(CWorkspaceCaptureProtocolTest, ValidModesCreateIndependentSourcesAndManagerMayBeDestroyedFirst) {
    createSource(5, HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_WINDOWS_ONLY);
    createSource(6, HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING);
    auto first  = source(5);
    auto second = source(6);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_NE(first, second);
    EXPECT_TRUE(first->good());
    EXPECT_TRUE(second->good());
    EXPECT_EQ(first->getTypeName(), "workspace");
    EXPECT_EQ(first->getName(), "73");
    EXPECT_EQ(first->logicalBox(), CBox(100, 200, 540, 960));

    request({3, (8U << 16) | 1U}); // manager.destroy
    EXPECT_EQ(wl_client_get_object(m_client, 3), nullptr);
    EXPECT_EQ(source(5), first);
    EXPECT_EQ(source(6), second);

    auto next              = makeMonitor({1280, 720});
    next->m_position       = {300, 400};
    m_workspace->m_monitor = next;
    m_workspace->m_events.monitorChanged.emit();
    EXPECT_EQ(first->logicalBox(), CBox(300, 400, 1280, 720));
    EXPECT_EQ(second->logicalBox(), CBox(300, 400, 1280, 720));

    m_workspace->renameForCapture("retained");
    PHLWORKSPACEREF workspace = m_workspace;
    m_workspace.reset();
    EXPECT_TRUE(workspace.expired());
    EXPECT_EQ(first->getName(), "retained");
    EXPECT_EQ(second->getTypeName(), "workspace");
    EXPECT_EQ(first->logicalBox(), CBox(300, 400, 1280, 720));
    PHLMONITORREF monitor = next;
    next.reset();
    EXPECT_TRUE(monitor.expired());
    EXPECT_EQ(first->logicalBox(), CBox());
    EXPECT_EQ(first->getTypeName(), "workspace");

    WP<CImageCaptureSource> weakFirst = first, weakSecond = second;
    first.reset();
    second.reset();
    request({5, 8U << 16}); // source.destroy
    EXPECT_EQ(wl_client_get_object(m_client, 5), nullptr);
    EXPECT_TRUE(weakFirst.expired());
    EXPECT_FALSE(weakSecond.expired());
    request({6, 8U << 16});
    EXPECT_TRUE(weakSecond.expired());
}

TEST_F(CWorkspaceCaptureProtocolTest, ClientDisconnectReleasesSources) {
    createSource(5, HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_WINDOWS_ONLY);
    ASSERT_TRUE(source(5));
    WP<CImageCaptureSource> weak = source(5);
    m_socket.reset();
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    EXPECT_TRUE(weak.expired());
}

class CWorkspaceCaptureInvalidModeTest : public CWorkspaceCaptureProtocolTest, public ::testing::WithParamInterface<uint32_t> {};

TEST_P(CWorkspaceCaptureInvalidModeTest, RejectsUnknownModeWithProtocolError) {
    createSource(5, GetParam());
    bool receivedError = false;
    receiveEvents([&](const auto& event) {
        if (event[0] != 1 || (event[1] & 0xFFFF) != 0)
            return;
        ASSERT_GE(event.size(), 5U);
        receivedError = true;
        EXPECT_EQ(event[2], 3U);
        EXPECT_EQ(event[3], sc<uint32_t>(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_ERROR_INVALID_MODE));
    });
    EXPECT_TRUE(receivedError);
}

INSTANTIATE_TEST_SUITE_P(Modes, CWorkspaceCaptureInvalidModeTest, ::testing::Values(2U, UINT32_MAX));
