#include <protocols/core/Output.hpp>
#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <output/Monitor.hpp>
#include <render/Renderer.hpp>

#include <gtest/gtest.h>
#include <hyprutils/os/FileDescriptor.hpp>

#include <array>
#include <memory>
#include <utility>
#include <sys/socket.h>

using namespace Hyprutils::OS;

class COutputTestOutput : public Aquamarine::IOutput {
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

// Exercise real wl_output bindings and requests on an isolated Wayland server.
class COutputTest : public ::testing::Test {
  protected:
    void                                                       SetUp() override;
    void                                                       TearDown() override;
    PHLMONITOR                                                 makeMonitor();
    SP<CWLOutputProtocol>                                      makeProtocol(PHLMONITOR monitor);
    void                                                       removeAndExpireMonitor();
    void                                                       bindOutput();
    void                                                       releaseOutput();
    void                                                       disconnectClient();

    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    wl_client*                                                 m_client = nullptr; // Owned by m_display.
    PHLMONITOR                                                 m_monitor;
    SP<CWLOutputProtocol>                                      m_protocol;
    WP<CWLOutputResource>                                      m_resource;

  private:
    bool                                 m_installedGlobals = false;
    UP<CCompositor>                      m_previousCompositor;
    UP<CEventLoopManager>                m_previousEventLoop;
    UP<Render::IHyprRenderer>            m_previousRenderer;
    UP<Event::CEventBus>                 m_previousEventBus;
    UP<Config::IConfigManager>           m_previousConfig;
    UP<Config::CConfigWatcher>           m_previousWatcher;
    UP<Animation::CHyprAnimationManager> m_previousAnimations;
    UP<Config::CAnimationTreeController> m_previousTree;
    decltype(PROTO::outputs)             m_previousOutputs;
    decltype(PROTO::defunctOutputs)      m_previousDefunctOutputs;
};

void COutputTest::SetUp() {
    m_display.reset(wl_display_create());
    ASSERT_NE(m_display, nullptr);

    std::array<int, 2> sockets = {
        -1,
        -1,
    };
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor serverSocket{sockets[0]};
    m_socket = CFileDescriptor{sockets[1]};
    m_client = wl_client_create(m_display.get(), serverSocket.get());
    ASSERT_NE(m_client, nullptr);
    serverSocket.take(); // wl_client owns the socket now.

    m_previousCompositor = std::move(g_pCompositor);
    m_previousEventLoop  = std::move(g_pEventLoopManager);
    m_previousRenderer   = std::move(g_pHyprRenderer);
    m_previousEventBus   = std::move(Event::bus());
    m_previousConfig     = std::move(Config::mgr());
    m_previousWatcher    = std::move(Config::watcher());
    m_previousAnimations = std::move(Animation::mgr());
    m_previousTree       = std::move(Config::animationTree());
    m_previousOutputs.swap(PROTO::outputs);
    m_previousDefunctOutputs.swap(PROTO::defunctOutputs);
    m_installedGlobals = true;

    Event::bus()                 = makeUnique<Event::CEventBus>();
    g_pCompositor                = makeUnique<CCompositor>(true);
    g_pCompositor->m_wlDisplay   = m_display.get();
    g_pCompositor->m_wlEventLoop = wl_display_get_event_loop(m_display.get());
    // Only construct config defaults; the dummy path is never loaded.
    g_pCompositor->m_explicitConfigPath = "output-test.lua";
    Config::Supplementary::Jeremy::flushCachedCfgPath();
    Config::watcher() = makeUnique<Config::CConfigWatcher>();
    Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
    CConfigValueBase::flushCaches();
    Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
    Config::animationTree() = makeUnique<Config::CAnimationTreeController>();

    m_monitor  = makeMonitor();
    m_protocol = makeProtocol(m_monitor);
    ASSERT_NE(m_protocol->getGlobal(), nullptr);
    PROTO::outputs.emplace(m_monitor->m_name, m_protocol);
}

void COutputTest::TearDown() {
    if (m_display)
        wl_display_destroy_clients(m_display.get());
    m_client = nullptr;
    m_socket.reset();
    if (!m_installedGlobals)
        return;

    m_protocol.reset();
    PROTO::outputs.clear();
    PROTO::defunctOutputs.clear();
    m_monitor.reset();
    Animation::mgr().reset();
    Config::animationTree().reset();
    Config::mgr().reset();
    Config::watcher().reset();
    g_pCompositor.reset();
    Event::bus().reset();

    PROTO::outputs.swap(m_previousOutputs);
    PROTO::defunctOutputs.swap(m_previousDefunctOutputs);
    Animation::mgr()        = std::move(m_previousAnimations);
    Config::animationTree() = std::move(m_previousTree);
    Config::mgr()           = std::move(m_previousConfig);
    Config::watcher()       = std::move(m_previousWatcher);
    if (Config::mgr())
        CConfigValueBase::flushCaches();
    g_pCompositor       = std::move(m_previousCompositor);
    g_pEventLoopManager = std::move(m_previousEventLoop);
    g_pHyprRenderer     = std::move(m_previousRenderer);
    Event::bus()        = std::move(m_previousEventBus);
    Config::Supplementary::Jeremy::flushCachedCfgPath();
}

PHLMONITOR COutputTest::makeMonitor() {
    auto output     = makeShared<COutputTestOutput>();
    output->name    = "output-test";
    auto monitor    = makeShared<Monitor::CMonitor>(output);
    monitor->m_self = monitor;
    return monitor;
}

SP<CWLOutputProtocol> COutputTest::makeProtocol(PHLMONITOR monitor) {
    auto protocol    = makeShared<CWLOutputProtocol>(&wl_output_interface, 4, "wl_output", monitor);
    protocol->m_self = protocol;
    return protocol;
}

void COutputTest::removeAndExpireMonitor() {
    m_protocol->remove();
    m_monitor.reset();
    ASSERT_TRUE(m_protocol->isDefunct());
    ASSERT_TRUE(m_protocol->m_monitor.expired());
}

void COutputTest::bindOutput() {
    m_protocol->bindManager(m_client, nullptr, 4, 2);
    ASSERT_NE(wl_client_get_object(m_client, 2), nullptr);
    m_resource = CWLOutputResource::fromResource(wl_client_get_object(m_client, 2));
    ASSERT_FALSE(m_resource.expired());
    EXPECT_TRUE(m_resource->good());
    EXPECT_EQ(m_resource->client(), m_client);
    EXPECT_EQ(m_resource->m_owner.lock(), m_protocol);
    EXPECT_TRUE(m_protocol->hasBoundResources());
    EXPECT_EQ(m_protocol->outputResourcesFrom(m_client).size(), 1U);
}

void COutputTest::releaseOutput() {
    // wl_output.release: object 2, eight-byte request, opcode 0 (since v3).
    const std::array<uint32_t, 2> request = {
        2U,
        8U << 16,
    };
    ASSERT_EQ(send(m_socket.get(), request.data(), sizeof(request), MSG_NOSIGNAL), sc<ssize_t>(sizeof(request)));
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    EXPECT_EQ(wl_client_get_object(m_client, 2), nullptr);
}

void COutputTest::disconnectClient() {
    m_socket.reset();
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    m_client = nullptr;
}

TEST_F(COutputTest, LateBindAfterMonitorExpiresDrainsOnRelease) {
    ASSERT_NO_FATAL_FAILURE(removeAndExpireMonitor());
    ASSERT_NO_FATAL_FAILURE(bindOutput());

    ASSERT_NO_FATAL_FAILURE(releaseOutput());
    EXPECT_TRUE(m_resource.expired());
    EXPECT_FALSE(m_protocol->hasBoundResources());
    EXPECT_TRUE(m_protocol->outputResourcesFrom(m_client).empty());
    EXPECT_TRUE(PROTO::outputs.empty());
    EXPECT_TRUE(PROTO::defunctOutputs.empty());
}

TEST_F(COutputTest, LateBindAfterMonitorExpiresDrainsOnDisconnect) {
    ASSERT_NO_FATAL_FAILURE(removeAndExpireMonitor());
    ASSERT_NO_FATAL_FAILURE(bindOutput());

    ASSERT_NO_FATAL_FAILURE(disconnectClient());
    EXPECT_TRUE(m_resource.expired());
    EXPECT_FALSE(m_protocol->hasBoundResources());
    EXPECT_TRUE(PROTO::outputs.empty());
    EXPECT_TRUE(PROTO::defunctOutputs.empty());
}

TEST_F(COutputTest, DrainingDefunctOutputPreservesSameNameReplacement) {
    ASSERT_NO_FATAL_FAILURE(removeAndExpireMonitor());
    ASSERT_NO_FATAL_FAILURE(bindOutput());

    m_monitor        = makeMonitor();
    auto replacement = makeProtocol(m_monitor);
    ASSERT_NE(replacement->getGlobal(), nullptr);
    PROTO::defunctOutputs.emplace_back(m_protocol);
    PROTO::outputs.at(m_monitor->m_name) = replacement;

    // Only defunctOutputs owns the old protocol while its last resource drains.
    WP<CWLOutputProtocol> oldProtocol = m_protocol;
    m_protocol.reset();
    ASSERT_FALSE(oldProtocol.expired());
    ASSERT_EQ(PROTO::defunctOutputs.size(), 1U);

    ASSERT_NO_FATAL_FAILURE(releaseOutput());
    EXPECT_TRUE(m_resource.expired());
    EXPECT_TRUE(oldProtocol.expired());
    EXPECT_TRUE(PROTO::defunctOutputs.empty());
    ASSERT_EQ(PROTO::outputs.size(), 1U);
    ASSERT_TRUE(PROTO::outputs.contains(m_monitor->m_name));
    EXPECT_EQ(PROTO::outputs.at(m_monitor->m_name), replacement);
    EXPECT_FALSE(replacement->isDefunct());
}
