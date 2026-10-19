#include <protocols/core/Output.hpp>
#include <protocols/core/Compositor.hpp>
#include <protocols/XDGOutput.hpp>
#include <protocols/OutputPower.hpp>
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
#include <cerrno>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <sys/socket.h>

using namespace Hyprutils::OS;

struct SOutputTestEvent {
    uint32_t              objectID = 0;
    uint32_t              opcode   = 0;
    std::vector<uint32_t> payload;

    bool                  operator==(const SOutputTestEvent&) const = default;
};

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
    void                                                       bindOutput(uint32_t version = 4);
    void                                                       readOutputEvents(std::vector<uint32_t>& opcodes);
    void                                                       readOutputEvents(std::vector<SOutputTestEvent>& events);
    void                                                       releaseOutput();
    void                                                       disconnectClient();
    void                                                       drainOutput(uint32_t version);

    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    wl_client*                                                 m_client = nullptr; // Owned by m_display.
    PHLMONITOR                                                 m_monitor;
    SP<CWLOutputProtocol>                                      m_protocol;
    WP<CWLOutputResource>                                      m_resource;
    int                                                        m_boundCount = 0;

  private:
    bool                                 m_installedGlobals = false;
    CHyprSignalListener                  m_outputBound;
    UP<CCompositor>                      m_previousCompositor;
    UP<CWLCompositorProtocol>            m_previousCompositorProtocol;
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

    m_previousCompositor         = std::move(g_pCompositor);
    m_previousCompositorProtocol = std::move(PROTO::compositor);
    m_previousEventLoop          = std::move(g_pEventLoopManager);
    m_previousRenderer           = std::move(g_pHyprRenderer);
    m_previousEventBus           = std::move(Event::bus());
    m_previousConfig             = std::move(Config::mgr());
    m_previousWatcher            = std::move(Config::watcher());
    m_previousAnimations         = std::move(Animation::mgr());
    m_previousTree               = std::move(Config::animationTree());
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
    PROTO::compositor       = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "wl_compositor");
    ASSERT_NE(PROTO::compositor->getGlobal(), nullptr);

    m_monitor  = makeMonitor();
    m_protocol = makeProtocol(m_monitor);
    ASSERT_NE(m_protocol->getGlobal(), nullptr);
    PROTO::outputs.emplace(m_monitor->m_name, m_protocol);
    m_outputBound = m_protocol->m_events.outputBound.listen([this](SP<CWLOutputResource> resource) {
        ++m_boundCount;
        EXPECT_TRUE(resource->good());
        EXPECT_EQ(resource->client(), m_client);
        EXPECT_EQ(resource->m_owner.lock(), m_protocol);
        EXPECT_EQ(resource->m_self.lock(), resource);
    });
}

void COutputTest::TearDown() {
    if (m_display)
        wl_display_destroy_clients(m_display.get());
    m_client = nullptr;
    m_socket.reset();
    if (!m_installedGlobals)
        return;

    m_outputBound.reset();
    m_protocol.reset();
    PROTO::outputs.clear();
    PROTO::defunctOutputs.clear();
    PROTO::compositor.reset();
    m_monitor.reset();
    Animation::mgr().reset();
    Config::animationTree().reset();
    Config::mgr().reset();
    Config::watcher().reset();
    g_pCompositor.reset();
    Event::bus().reset();

    PROTO::outputs.swap(m_previousOutputs);
    PROTO::defunctOutputs.swap(m_previousDefunctOutputs);
    PROTO::compositor       = std::move(m_previousCompositorProtocol);
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
    auto output         = makeShared<COutputTestOutput>();
    output->name        = "output-test";
    auto monitor        = makeShared<Monitor::CMonitor>(output);
    monitor->m_self     = monitor;
    monitor->m_position = {0, 0};
    monitor->m_size = monitor->m_pixelSize = monitor->m_transformedSize = {640, 480};
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

void COutputTest::bindOutput(uint32_t version) {
    m_protocol->bindManager(m_client, nullptr, version, 2);
    ASSERT_NE(wl_client_get_object(m_client, 2), nullptr);
    m_resource = CWLOutputResource::fromResource(wl_client_get_object(m_client, 2));
    ASSERT_FALSE(m_resource.expired());
    EXPECT_TRUE(m_resource->good());
    EXPECT_EQ(m_resource->client(), m_client);
    EXPECT_EQ(m_resource->m_owner.lock(), m_protocol);
    EXPECT_TRUE(m_protocol->hasBoundResources());
    EXPECT_EQ(m_protocol->outputResourcesFrom(m_client).size(), 1U);
}

void COutputTest::readOutputEvents(std::vector<uint32_t>& opcodes) {
    opcodes.clear();
    std::vector<SOutputTestEvent> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    for (const auto& event : events) {
        ASSERT_EQ(event.objectID, 2U);
        opcodes.emplace_back(event.opcode);
    }
}

void COutputTest::readOutputEvents(std::vector<SOutputTestEvent>& events) {
    events.clear();
    wl_client_flush(m_client);
    std::vector<uint8_t>      bytes;
    std::array<uint8_t, 1024> chunk = {};
    while (true) {
        const auto count = recv(m_socket.get(), chunk.data(), chunk.size(), MSG_DONTWAIT);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        ASSERT_GT(count, 0);
        bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + count);
    }

    for (size_t offset = 0; offset < bytes.size();) {
        std::array<uint32_t, 2> header = {};
        ASSERT_GE(bytes.size() - offset, sizeof(header));
        std::memcpy(header.data(), bytes.data() + offset, sizeof(header));
        const auto size = header[1] >> 16;
        ASSERT_GE(size, sizeof(header));
        ASSERT_EQ(size % sizeof(uint32_t), 0U);
        ASSERT_LE(size, bytes.size() - offset);
        SOutputTestEvent event{
            .objectID = header[0],
            .opcode   = header[1] & 0xFFFFU,
            .payload  = std::vector<uint32_t>((size - sizeof(header)) / sizeof(uint32_t)),
        };
        if (!event.payload.empty())
            std::memcpy(event.payload.data(), bytes.data() + offset + sizeof(header), size - sizeof(header));
        events.emplace_back(std::move(event));
        offset += size;
    }
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

void COutputTest::drainOutput(uint32_t version) {
    if (version >= 3)
        ASSERT_NO_FATAL_FAILURE(releaseOutput());
    else
        ASSERT_NO_FATAL_FAILURE(disconnectClient()); // release was introduced in v3.
    EXPECT_TRUE(m_resource.expired());
    EXPECT_FALSE(m_protocol->hasBoundResources());
}

TEST_F(COutputTest, LateBindAfterMonitorExpiresDrainsOnRelease) {
    ASSERT_NO_FATAL_FAILURE(removeAndExpireMonitor());
    ASSERT_NO_FATAL_FAILURE(bindOutput());
    EXPECT_EQ(m_boundCount, 0);
    std::vector<uint32_t> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_TRUE(events.empty());

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
    EXPECT_EQ(m_boundCount, 0);

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

static std::vector<uint32_t> outputStateEvents(uint32_t version) {
    std::vector<uint32_t> events;
    if (version >= 2)
        events.emplace_back(WL_OUTPUT_SCALE);
    events.emplace_back(WL_OUTPUT_MODE);
    events.emplace_back(WL_OUTPUT_GEOMETRY);
    if (version >= 2)
        events.emplace_back(WL_OUTPUT_DONE);
    return events;
}

class COutputVersionTest : public COutputTest, public ::testing::WithParamInterface<uint32_t> {};

TEST_P(COutputVersionTest, RetainedMonitorWithoutBackendBindsSilentlyAndDrains) {
    m_monitor->m_output.reset();
    ASSERT_FALSE(m_protocol->isDefunct());
    ASSERT_NO_FATAL_FAILURE(bindOutput(GetParam()));
    EXPECT_EQ(m_boundCount, 0);
    std::vector<uint32_t> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_TRUE(events.empty());

    ASSERT_NO_FATAL_FAILURE(drainOutput(GetParam()));
    EXPECT_EQ(PROTO::outputs.at(m_monitor->m_name), m_protocol);
}

TEST_P(COutputVersionTest, DefunctProtocolWithLiveBackendBindsSilentlyAndDrains) {
    m_protocol->remove();
    ASSERT_TRUE(m_monitor->m_output);
    ASSERT_NO_FATAL_FAILURE(bindOutput(GetParam()));
    EXPECT_EQ(m_boundCount, 0);
    std::vector<uint32_t> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_TRUE(events.empty());

    ASSERT_NO_FATAL_FAILURE(drainOutput(GetParam()));
    EXPECT_TRUE(PROTO::outputs.empty());
    EXPECT_TRUE(PROTO::defunctOutputs.empty());
}

TEST_P(COutputVersionTest, LiveOutputEmitsVersionedStateUntilBackendDisconnects) {
    const auto version = GetParam();
    ASSERT_NO_FATAL_FAILURE(bindOutput(version));
    EXPECT_EQ(m_boundCount, 1);
    auto initialEvents = outputStateEvents(version);
    if (version >= 4)
        initialEvents.insert(initialEvents.begin(), {WL_OUTPUT_NAME, WL_OUTPUT_DESCRIPTION});
    std::vector<uint32_t> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_EQ(events, initialEvents);

    m_monitor->m_pixelSize   = {800, 600};
    m_monitor->m_refreshRate = 75;
    m_monitor->m_events.modeChanged.emit();
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_EQ(events, outputStateEvents(version));
    EXPECT_EQ(m_boundCount, 1);

    m_monitor->m_output.reset();
    m_resource->updateState();
    m_monitor->m_events.modeChanged.emit();
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_TRUE(events.empty());
    EXPECT_TRUE(m_resource->good());
    EXPECT_EQ(m_boundCount, 1);
    ASSERT_NO_FATAL_FAILURE(drainOutput(version));
}

INSTANTIATE_TEST_SUITE_P(Versions, COutputVersionTest, ::testing::Values(1U, 2U, 3U, 4U));

class CXDGOutputTest : public COutputTest {
  protected:
    void SetUp() override;
    void TearDown() override;
    void bindXDGManager(uint32_t xdgVersion = 3, uint32_t outputVersion = 4);
    void requestXDGOutput(uint32_t id = 4);
    void destroyXDGOutput();

  private:
    bool                   m_installedXDGOutput = false;
    UP<CXDGOutputProtocol> m_previousXDGOutput;
};

void CXDGOutputTest::SetUp() {
    ASSERT_NO_FATAL_FAILURE(COutputTest::SetUp());
    m_previousXDGOutput  = std::move(PROTO::xdgOutput);
    m_installedXDGOutput = true;
    PROTO::xdgOutput     = makeUnique<CXDGOutputProtocol>(&zxdg_output_manager_v1_interface, 3, "zxdg_output_manager_v1");
    ASSERT_NE(PROTO::xdgOutput->getGlobal(), nullptr);

    // Distinguish the backend description and logical geometry from core output state.
    m_monitor->m_output->description = "Backend output description";
    m_monitor->m_description         = "Monitor description";
    m_monitor->m_position            = {-80, 40};
    m_monitor->m_size                = {320, 240};
    m_monitor->m_scale               = 2;
}

void CXDGOutputTest::TearDown() {
    // XDG resource destruction dispatches through PROTO::xdgOutput.
    if (m_display)
        wl_display_destroy_clients(m_display.get());
    m_client = nullptr;
    if (m_installedXDGOutput)
        PROTO::xdgOutput = std::move(m_previousXDGOutput);
    COutputTest::TearDown();
}

void CXDGOutputTest::bindXDGManager(uint32_t xdgVersion, uint32_t outputVersion) {
    // Bind while healthy: later requests refer to this already-existing wl_output.
    ASSERT_NO_FATAL_FAILURE(bindOutput(outputVersion));
    ASSERT_EQ(m_boundCount, 1);
    std::vector<uint32_t> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    auto initialEvents = outputStateEvents(outputVersion);
    if (outputVersion >= 4)
        initialEvents.insert(initialEvents.begin(), {WL_OUTPUT_NAME, WL_OUTPUT_DESCRIPTION});
    ASSERT_EQ(events, initialEvents);

    PROTO::xdgOutput->bindManager(m_client, nullptr, xdgVersion, 3);
    ASSERT_NE(wl_client_get_object(m_client, 3), nullptr);
}

void CXDGOutputTest::requestXDGOutput(uint32_t id) {
    // zxdg_output_manager_v1.get_xdg_output(new_id, wl_output 2).
    const std::array<uint32_t, 4> request = {
        3U,
        (16U << 16) | 1U,
        id,
        2U,
    };
    ASSERT_EQ(send(m_socket.get(), request.data(), sizeof(request), MSG_NOSIGNAL), sc<ssize_t>(sizeof(request)));
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    auto resource = wl_client_get_object(m_client, id);
    ASSERT_NE(resource, nullptr);
    EXPECT_STREQ(wl_resource_get_class(resource), "zxdg_output_v1");
    EXPECT_EQ(wl_resource_get_version(resource), wl_resource_get_version(wl_client_get_object(m_client, 3)));
}

void CXDGOutputTest::destroyXDGOutput() {
    // zxdg_output_v1.destroy: object 4, eight-byte request, opcode 0.
    const std::array<uint32_t, 2> request = {
        4U,
        8U << 16,
    };
    ASSERT_EQ(send(m_socket.get(), request.data(), sizeof(request), MSG_NOSIGNAL), sc<ssize_t>(sizeof(request)));
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    EXPECT_EQ(wl_client_get_object(m_client, 4), nullptr);
    EXPECT_NE(wl_client_get_object(m_client, 3), nullptr);
    EXPECT_FALSE(m_resource.expired());

    // delete_id may remain queued until another event is posted.
    std::vector<SOutputTestEvent> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
}

enum class eXDGOutputInactiveState : uint8_t {
    MISSING_BACKEND,
    DEFUNCT_MISSING_BACKEND,
    EXPIRED_MONITOR,
    DEFUNCT_LIVE_BACKEND,
};

class CXDGOutputInactiveTest : public CXDGOutputTest, public ::testing::WithParamInterface<eXDGOutputInactiveState> {
  protected:
    void deactivateOutput();
};

void CXDGOutputInactiveTest::deactivateOutput() {
    switch (GetParam()) {
        case eXDGOutputInactiveState::MISSING_BACKEND:
            m_monitor->m_output.reset();
            ASSERT_FALSE(m_protocol->isDefunct());
            break;
        case eXDGOutputInactiveState::DEFUNCT_MISSING_BACKEND:
            m_protocol->remove();
            m_monitor->m_output.reset();
            ASSERT_TRUE(m_protocol->isDefunct());
            break;
        case eXDGOutputInactiveState::EXPIRED_MONITOR: ASSERT_NO_FATAL_FAILURE(removeAndExpireMonitor()); break;
        case eXDGOutputInactiveState::DEFUNCT_LIVE_BACKEND:
            m_protocol->remove();
            ASSERT_TRUE(m_protocol->isDefunct());
            ASSERT_TRUE(m_monitor->m_output);
            break;
    }
    ASSERT_FALSE(m_resource.expired());
    ASSERT_TRUE(m_resource->good());
}

TEST_P(CXDGOutputInactiveTest, RequestOnExistingOutputIsInertAndDrains) {
    ASSERT_NO_FATAL_FAILURE(bindXDGManager());
    ASSERT_NO_FATAL_FAILURE(deactivateOutput());
    ASSERT_NO_FATAL_FAILURE(requestXDGOutput());
    std::vector<SOutputTestEvent> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    ASSERT_TRUE(events.empty());

    // Call directly: the protocol constructor's bus listeners are static across fixtures.
    PROTO::xdgOutput->updateAllOutputs();
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_TRUE(events.empty());
    ASSERT_NO_FATAL_FAILURE(destroyXDGOutput());

    // Use a fresh ID to cover client teardown without waiting for delete_id.
    ASSERT_NO_FATAL_FAILURE(requestXDGOutput(5));
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_TRUE(events.empty());
    ASSERT_NO_FATAL_FAILURE(disconnectClient());
    EXPECT_TRUE(m_resource.expired());
    EXPECT_FALSE(m_protocol->hasBoundResources());
}

TEST_P(CXDGOutputInactiveTest, ExistingXDGOutputStopsUpdatingAfterRemoval) {
    ASSERT_NO_FATAL_FAILURE(bindXDGManager());
    ASSERT_NO_FATAL_FAILURE(requestXDGOutput());
    std::vector<SOutputTestEvent> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    ASSERT_FALSE(events.empty());

    ASSERT_NO_FATAL_FAILURE(deactivateOutput());
    PROTO::xdgOutput->updateAllOutputs();
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    EXPECT_TRUE(events.empty());
    ASSERT_NO_FATAL_FAILURE(destroyXDGOutput());
    ASSERT_NO_FATAL_FAILURE(drainOutput(4));
}

INSTANTIATE_TEST_SUITE_P(RemovedOutputs, CXDGOutputInactiveTest,
                         ::testing::Values(eXDGOutputInactiveState::MISSING_BACKEND, eXDGOutputInactiveState::DEFUNCT_MISSING_BACKEND, eXDGOutputInactiveState::EXPIRED_MONITOR,
                                           eXDGOutputInactiveState::DEFUNCT_LIVE_BACKEND));

static void expectOutputStringEvent(const SOutputTestEvent& event, uint32_t objectID, uint32_t opcode, const std::string& value) {
    EXPECT_EQ(event.objectID, objectID);
    EXPECT_EQ(event.opcode, opcode);
    const auto length = value.size() + 1;
    ASSERT_EQ(event.payload.size(), 1 + (length + 3) / sizeof(uint32_t));
    EXPECT_EQ(event.payload[0], length);
    // Wayland string padding is unspecified; compare only the string and its terminator.
    EXPECT_EQ(std::memcmp(event.payload.data() + 1, value.c_str(), length), 0);
}

class CXDGOutputVersionTest : public CXDGOutputTest, public ::testing::WithParamInterface<std::pair<uint32_t, uint32_t>> {};

TEST_P(CXDGOutputVersionTest, HealthyOutputPreservesVersionedEvents) {
    const auto [xdgVersion, outputVersion] = GetParam();
    ASSERT_NO_FATAL_FAILURE(bindXDGManager(xdgVersion, outputVersion));
    ASSERT_NO_FATAL_FAILURE(requestXDGOutput());
    std::vector<SOutputTestEvent> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    if (xdgVersion >= 2) {
        ASSERT_GE(events.size(), 2U);
        // zxdg_output_v1.name and description have opcodes 3 and 4.
        ASSERT_NO_FATAL_FAILURE(expectOutputStringEvent(events[0], 4, 3, m_monitor->m_name));
        ASSERT_NO_FATAL_FAILURE(expectOutputStringEvent(events[1], 4, 4, m_monitor->m_output->description));
        events.erase(events.begin(), events.begin() + 2);
    }

    // zxdg_output_v1.logical_position and logical_size have opcodes 0 and 1.
    std::vector<SOutputTestEvent> expected = {
        {4U, 0U, {sc<uint32_t>(-80), 40U}},
        {4U, 1U, {320U, 240U}},
    };
    if (xdgVersion < 3)
        expected.push_back({4U, 2U, {}}); // zxdg_output_v1.done
    else if (outputVersion >= 2)
        expected.push_back({2U, WL_OUTPUT_DONE, {}});
    EXPECT_EQ(events, expected);

    ASSERT_NO_FATAL_FAILURE(destroyXDGOutput());
    ASSERT_NO_FATAL_FAILURE(drainOutput(outputVersion));
}

// XDG v1/v2 use their own done; v3 uses core done only if wl_output supports it.
INSTANTIATE_TEST_SUITE_P(Versions, CXDGOutputVersionTest, ::testing::Values(std::pair{1U, 4U}, std::pair{2U, 4U}, std::pair{3U, 1U}, std::pair{3U, 2U}));

class COutputPowerTest : public COutputTest {
  protected:
    void SetUp() override;
    void TearDown() override;
    void sendPowerRequest(std::initializer_list<uint32_t> request);
    void requestOutputPower(uint32_t id = 4);
    void setPowerMode(zwlrOutputPowerV1Mode mode);
    void destroyPowerResource(uint32_t id = 4);
    void expectPowerEvents(const std::vector<SOutputTestEvent>& expected = {});
    void expectFailedPower(uint32_t id = 4);
    void expectPowerStaysInert();

  private:
    bool                     m_installedOutputPower = false;
    UP<COutputPowerProtocol> m_previousOutputPower;
};

void COutputPowerTest::SetUp() {
    ASSERT_NO_FATAL_FAILURE(COutputTest::SetUp());
    m_previousOutputPower  = std::move(PROTO::outputPower);
    m_installedOutputPower = true;
    PROTO::outputPower     = makeUnique<COutputPowerProtocol>(&zwlr_output_power_manager_v1_interface, 1, "zwlr_output_power_manager_v1");
    ASSERT_NE(PROTO::outputPower->getGlobal(), nullptr);
    m_monitor->m_dpmsStatus = true;

    // Requests after removal still use an output bound while it was healthy.
    ASSERT_NO_FATAL_FAILURE(bindOutput());
    ASSERT_EQ(m_boundCount, 1);
    std::vector<uint32_t> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    auto initialEvents = outputStateEvents(4);
    initialEvents.insert(initialEvents.begin(), {WL_OUTPUT_NAME, WL_OUTPUT_DESCRIPTION});
    ASSERT_EQ(events, initialEvents);
    PROTO::outputPower->bindManager(m_client, nullptr, 1, 3);
    ASSERT_NE(wl_client_get_object(m_client, 3), nullptr);
}

void COutputPowerTest::TearDown() {
    // Resource destruction dispatches through PROTO::outputPower.
    if (m_display)
        wl_display_destroy_clients(m_display.get());
    m_client = nullptr;
    if (m_installedOutputPower)
        PROTO::outputPower = std::move(m_previousOutputPower);
    COutputTest::TearDown();
}

void COutputPowerTest::sendPowerRequest(std::initializer_list<uint32_t> request) {
    const auto size = request.size() * sizeof(uint32_t);
    ASSERT_EQ(send(m_socket.get(), request.begin(), size, MSG_NOSIGNAL), sc<ssize_t>(size));
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
}

void COutputPowerTest::requestOutputPower(uint32_t id) {
    // zwlr_output_power_manager_v1.get_output_power(new_id, wl_output 2), opcode 0.
    ASSERT_NO_FATAL_FAILURE(sendPowerRequest({3U, 16U << 16, id, 2U}));
    auto resource = wl_client_get_object(m_client, id);
    ASSERT_NE(resource, nullptr);
    EXPECT_STREQ(wl_resource_get_class(resource), "zwlr_output_power_v1");
    EXPECT_EQ(wl_resource_get_version(resource), 1);
}

void COutputPowerTest::setPowerMode(zwlrOutputPowerV1Mode mode) {
    // zwlr_output_power_v1.set_mode, opcode 0.
    ASSERT_NO_FATAL_FAILURE(sendPowerRequest({4U, 12U << 16, mode}));
}

void COutputPowerTest::destroyPowerResource(uint32_t id) {
    // Both the manager and power object use destroy opcode 1.
    ASSERT_NO_FATAL_FAILURE(sendPowerRequest({id, (8U << 16) | 1U}));
    ASSERT_EQ(wl_client_get_object(m_client, id), nullptr);
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents());
}

void COutputPowerTest::expectPowerEvents(const std::vector<SOutputTestEvent>& expected) {
    std::vector<SOutputTestEvent> events;
    ASSERT_NO_FATAL_FAILURE(readOutputEvents(events));
    // modeChanged also updates wl_output; delete_id may flush with a later event.
    // Keep display errors and every other unexpected object/event visible.
    std::erase_if(events, [](const auto& event) { return event.objectID == 2 || (event.objectID == 1 && event.opcode == WL_DISPLAY_DELETE_ID); });
    ASSERT_EQ(events, expected);
}

void COutputPowerTest::expectFailedPower(uint32_t id) {
    ASSERT_NE(wl_client_get_object(m_client, id), nullptr);
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents({{id, 1U, {}}})); // failed, without mode
}

void COutputPowerTest::expectPowerStaysInert() {
    const auto status = m_monitor->m_dpmsStatus;
    const auto mode   = status ? ZWLR_OUTPUT_POWER_V1_MODE_OFF : ZWLR_OUTPUT_POWER_V1_MODE_ON;
    ASSERT_NO_FATAL_FAILURE(setPowerMode(mode));
    ASSERT_NO_FATAL_FAILURE(setPowerMode(mode));
    ASSERT_EQ(m_monitor->m_dpmsStatus, status);
    m_monitor->m_events.dpmsChanged.emit();
    m_monitor->m_events.modeChanged.emit();
    Event::bus()->m_events.monitor.destroyMon.emit(m_monitor);
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents());
}

class COutputPowerUnavailableTest : public COutputPowerTest, public ::testing::WithParamInterface<bool> {};

TEST_P(COutputPowerUnavailableTest, InitialFailureIsInertAndDrains) {
    if (GetParam())
        ASSERT_NO_FATAL_FAILURE(removeAndExpireMonitor());
    else
        m_monitor->m_output.reset();
    ASSERT_NO_FATAL_FAILURE(requestOutputPower());
    ASSERT_NO_FATAL_FAILURE(expectFailedPower());
    ASSERT_NO_FATAL_FAILURE(setPowerMode(ZWLR_OUTPUT_POWER_V1_MODE_OFF));
    ASSERT_NO_FATAL_FAILURE(setPowerMode(ZWLR_OUTPUT_POWER_V1_MODE_ON));
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents());
    EXPECT_TRUE(!m_monitor || m_monitor->m_dpmsStatus);
    ASSERT_NO_FATAL_FAILURE(destroyPowerResource());
    EXPECT_FALSE(m_resource.expired());

    // Fresh ID: no dependence on immediate delivery of delete_id for object 4.
    ASSERT_NO_FATAL_FAILURE(requestOutputPower(5));
    ASSERT_NO_FATAL_FAILURE(expectFailedPower(5));
    ASSERT_NO_FATAL_FAILURE(disconnectClient());
    EXPECT_TRUE(m_resource.expired());
    EXPECT_FALSE(m_protocol->hasBoundResources());
}

INSTANTIATE_TEST_SUITE_P(Unavailable, COutputPowerUnavailableTest, ::testing::Values(false, true));

enum class eOutputPowerFirstNotification : uint8_t {
    SET_MODE,
    DPMS_CHANGED,
    MODE_CHANGED,
};

class COutputPowerBackendLossTest : public COutputPowerTest, public ::testing::WithParamInterface<eOutputPowerFirstNotification> {};

TEST_P(COutputPowerBackendLossTest, FailureIsSentOnceAndBackendRestorationDoesNotRevive) {
    ASSERT_NO_FATAL_FAILURE(requestOutputPower());
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents({{4U, 0U, {ZWLR_OUTPUT_POWER_V1_MODE_ON}}}));
    auto backend = std::move(m_monitor->m_output);
    switch (GetParam()) {
        case eOutputPowerFirstNotification::SET_MODE: ASSERT_NO_FATAL_FAILURE(setPowerMode(ZWLR_OUTPUT_POWER_V1_MODE_OFF)); break;
        case eOutputPowerFirstNotification::DPMS_CHANGED: m_monitor->m_events.dpmsChanged.emit(); break;
        case eOutputPowerFirstNotification::MODE_CHANGED: m_monitor->m_events.modeChanged.emit(); break;
    }
    ASSERT_NO_FATAL_FAILURE(expectFailedPower());
    ASSERT_TRUE(m_monitor->m_dpmsStatus);
    ASSERT_NO_FATAL_FAILURE(expectPowerStaysInert());

    m_monitor->m_output     = std::move(backend);
    m_monitor->m_dpmsStatus = false;
    ASSERT_NO_FATAL_FAILURE(expectPowerStaysInert());
    ASSERT_NO_FATAL_FAILURE(destroyPowerResource());
    ASSERT_NO_FATAL_FAILURE(drainOutput(4));
}

INSTANTIATE_TEST_SUITE_P(FirstNotification, COutputPowerBackendLossTest,
                         ::testing::Values(eOutputPowerFirstNotification::SET_MODE, eOutputPowerFirstNotification::DPMS_CHANGED, eOutputPowerFirstNotification::MODE_CHANGED));

TEST_F(COutputPowerTest, OnlyMatchingMonitorDestructionFailsAndRetainedMonitorSignalsAreSafe) {
    ASSERT_NO_FATAL_FAILURE(requestOutputPower());
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents({{4U, 0U, {ZWLR_OUTPUT_POWER_V1_MODE_ON}}}));
    auto unrelated = makeMonitor();
    Event::bus()->m_events.monitor.destroyMon.emit(unrelated);
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents());

    Event::bus()->m_events.monitor.destroyMon.emit(m_monitor);
    ASSERT_NO_FATAL_FAILURE(expectFailedPower());
    ASSERT_NO_FATAL_FAILURE(expectPowerStaysInert());
    ASSERT_NO_FATAL_FAILURE(destroyPowerResource());
    m_monitor->m_events.dpmsChanged.emit();
    m_monitor->m_events.modeChanged.emit();
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents());
    ASSERT_NO_FATAL_FAILURE(disconnectClient());
    EXPECT_TRUE(m_resource.expired());
}

class COutputPowerModeTest : public COutputPowerTest, public ::testing::WithParamInterface<bool> {};

TEST_P(COutputPowerModeTest, HealthyModesAreForwardedAndChildOutlivesOutputAndManager) {
    m_monitor->m_dpmsStatus = GetParam();
    const auto initialMode  = GetParam() ? ZWLR_OUTPUT_POWER_V1_MODE_ON : ZWLR_OUTPUT_POWER_V1_MODE_OFF;
    const auto changedMode  = GetParam() ? ZWLR_OUTPUT_POWER_V1_MODE_OFF : ZWLR_OUTPUT_POWER_V1_MODE_ON;
    ASSERT_NO_FATAL_FAILURE(requestOutputPower());
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents({{4U, 0U, {initialMode}}}));
    // Same-mode requests return before animation or backend work in setDPMS.
    ASSERT_NO_FATAL_FAILURE(setPowerMode(initialMode));
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents());

    m_monitor->m_dpmsStatus = !GetParam();
    m_monitor->m_events.dpmsChanged.emit();
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents({{4U, 0U, {changedMode}}}));
    m_monitor->m_events.modeChanged.emit();
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents({{4U, 0U, {changedMode}}}));

    ASSERT_NO_FATAL_FAILURE(drainOutput(4));
    ASSERT_NO_FATAL_FAILURE(destroyPowerResource(3));
    ASSERT_NE(wl_client_get_object(m_client, 4), nullptr);
    m_monitor->m_dpmsStatus = GetParam();
    m_monitor->m_events.dpmsChanged.emit();
    m_monitor->m_events.modeChanged.emit();
    ASSERT_NO_FATAL_FAILURE(expectPowerEvents({{4U, 0U, {initialMode}}, {4U, 0U, {initialMode}}}));
    ASSERT_NO_FATAL_FAILURE(destroyPowerResource());
}

INSTANTIATE_TEST_SUITE_P(Modes, COutputPowerModeTest, ::testing::Values(false, true));
