#include <Compositor.hpp>
#include <managers/TokenManager.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/lua/LuaBindings.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <config/supplementary/propRefresher/PropRefresher.hpp>
#include <desktop/view/window/Window.hpp>
#include <desktop/view/window/WaylandBackend.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <protocols/LinuxDMABUF.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/core/Compositor.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#include <lauxlib.h>
}

// Synthetic format lists exercise feedback policy without a DRM device or GPU.
class CDMABUFFormatTableTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_previousCompositor                = std::move(g_pCompositor);
        m_previousConfig                    = std::move(Config::mgr());
        m_previousWatcher                   = std::move(Config::watcher());
        m_previousTokens                    = std::move(g_pTokenManager);
        g_pTokenManager                     = makeUnique<CTokenManager>();
        g_pCompositor                       = makeUnique<CCompositor>(true);
        g_pCompositor->m_explicitConfigPath = "dmabuf-feedback-test.lua";
        Config::Supplementary::Jeremy::flushCachedCfgPath();
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
    }

    void TearDown() override {
        Config::mgr()     = std::move(m_previousConfig);
        Config::watcher() = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pCompositor   = std::move(m_previousCompositor);
        g_pTokenManager = std::move(m_previousTokens);
        Config::Supplementary::Jeremy::flushCachedCfgPath();
    }

    void setFiltering(bool enabled) {
        CConfigValue<Config::BOOL> filtering{"quirks:skip_non_kms_dmabuf_formats"};
        *filtering.ptr() = enabled;
    }

    UP<CDMABUFFormatTable> makeTable(bool withMonitor = true) {
        SDMABUFTranche renderer{
            .device = 1,
            .formats =
                {
                    {DRM_FORMAT_ARGB8888, {I915_FORMAT_MOD_X_TILED}},
                    {DRM_FORMAT_XRGB8888, {I915_FORMAT_MOD_X_TILED, DRM_FORMAT_MOD_LINEAR}},
                },
        };
        std::vector<std::pair<PHLMONITORREF, SDMABUFTranche>> monitors;
        if (withMonitor)
            monitors.emplace_back(PHLMONITORREF{},
                                  SDMABUFTranche{
                                      .device  = 1,
                                      .flags   = ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT,
                                      .formats = {{DRM_FORMAT_XRGB8888, {I915_FORMAT_MOD_X_TILED}}},
                                  });
        return makeUnique<CDMABUFFormatTable>(renderer, monitors);
    }

    void expectFormats(const CDMABUFFormatTable& table, const SDMABUFTranche& tranche, const std::vector<std::pair<uint32_t, uint64_t>>& expected) {
        ASSERT_TRUE(table.m_tableFD.isValid());
        std::vector<SDMABUFFormatTableEntry> entries(table.m_tableSize / sizeof(SDMABUFFormatTableEntry));
        ASSERT_EQ(pread(table.m_tableFD.get(), entries.data(), table.m_tableSize, 0), sc<ssize_t>(table.m_tableSize));
        std::vector<std::pair<uint32_t, uint64_t>> actual;
        for (const auto index : tranche.indices) {
            ASSERT_LT(index, entries.size());
            actual.emplace_back(entries[index].fmt, entries[index].modifier);
        }
        EXPECT_EQ(actual, expected);
    }

  private:
    UP<CCompositor>            m_previousCompositor;
    UP<Config::IConfigManager> m_previousConfig;
    UP<Config::CConfigWatcher> m_previousWatcher;
    UP<CTokenManager>          m_previousTokens;
};

TEST_F(CDMABUFFormatTableTest, DefaultRetainsAlphaForOpaqueOnlyKMS) {
    const auto table = makeTable();
    expectFormats(*table, table->m_rendererTranche,
                  {
                      {DRM_FORMAT_ARGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR},
                  });
    expectFormats(*table, table->m_monitorTranches.front().second, {{DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED}});
}

TEST_F(CDMABUFFormatTableTest, ExplicitFilteringRequiresExactFormatAndModifier) {
    setFiltering(true);
    const auto table = makeTable();
    expectFormats(*table, table->m_rendererTranche, {{DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED}});
    expectFormats(*table, table->m_monitorTranches.front().second, {{DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED}});
}

TEST_F(CDMABUFFormatTableTest, DisablingFilteringRestoresRendererFormats) {
    setFiltering(true);
    setFiltering(false);
    const auto table = makeTable();
    expectFormats(*table, table->m_rendererTranche,
                  {
                      {DRM_FORMAT_ARGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR},
                  });
}

TEST_F(CDMABUFFormatTableTest, FilteringWithoutMonitorsRetainsRendererFormats) {
    setFiltering(true);
    const auto table = makeTable(false);
    expectFormats(*table, table->m_rendererTranche,
                  {
                      {DRM_FORMAT_ARGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED},
                      {DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR},
                  });
}

class CDMABUFFeedbackOutput : public Aquamarine::IOutput {
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
        return {{DRM_FORMAT_XRGB8888, {I915_FORMAT_MOD_X_TILED}}};
    }
    bool pendingPageFlip() override {
        return false;
    }
    bool pendingIdleFrame() override {
        return false;
    }
};

using TFeedbackFormats = std::vector<std::pair<uint32_t, uint64_t>>;

struct SFeedbackTranche {
    uint32_t         flags = 0;
    TFeedbackFormats formats;
    bool             operator==(const SFeedbackTranche&) const = default;
};

struct SFeedbackState {
    std::vector<SDMABUFFormatTableEntry> table;
    std::vector<SFeedbackTranche>        tranches;
    SFeedbackTranche                     pending;
    size_t                               tableEvents = 0;
    size_t                               doneEvents  = 0;
};

// Install synthetic tables through a narrow friend fixture, then exercise real
// Wayland requests and decode the table FDs/indices sent to existing clients.
class CDMABUFFeedbackTest : public CDMABUFFormatTableTest {
  protected:
    void SetUp() override {
        CDMABUFFormatTableTest::SetUp();
        m_previousBus                = std::move(Event::bus());
        m_previousProtocol           = std::move(PROTO::linuxDma);
        m_previousCompositorProtocol = std::move(PROTO::compositor);
        m_previousPresentation       = std::move(PROTO::presentation);
        m_previousAnimations         = std::move(Animation::mgr());
        m_previousTree               = std::move(Config::animationTree());
        Event::bus()                 = makeUnique<Event::CEventBus>();
        Animation::mgr()             = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree()      = makeUnique<Config::CAnimationTreeController>();

        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        g_pCompositor->m_wlDisplay   = m_display.get();
        g_pCompositor->m_wlEventLoop = wl_display_get_event_loop(m_display.get());
        std::array<int, 2> sockets{-1, -1};
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        Hyprutils::OS::CFileDescriptor serverSocket{sockets[0]};
        m_socket = Hyprutils::OS::CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), serverSocket.get());
        ASSERT_NE(m_client, nullptr);
        serverSocket.take(); // Owned by the Wayland display.

        m_monitor                                                       = makeShared<Monitor::CMonitor>(makeShared<CDMABUFFeedbackOutput>());
        m_monitor->m_self                                               = m_monitor;
        PROTO::compositor                                               = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "compositor-test");
        PROTO::presentation                                             = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "presentation-test");
        PROTO::linuxDma                                                 = makeUnique<CLinuxDMABufV1Protocol>(&zwp_linux_dmabuf_v1_interface, 5, "dmabuf-feedback-test");
        PROTO::linuxDma->m_mainDevice                                   = 1;
        PROTO::linuxDma->m_formatTable                                  = makeTable();
        PROTO::linuxDma->m_formatTable->m_monitorTranches.front().first = m_monitor;
        PROTO::linuxDma->bindManager(m_client, nullptr, 5, 2);
    }

    void TearDown() override {
        if (m_window)
            m_window->wlSurface()->unassign();
        m_window.reset();
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        m_surface.reset();
        PROTO::linuxDma     = std::move(m_previousProtocol);
        PROTO::compositor   = std::move(m_previousCompositorProtocol);
        PROTO::presentation = std::move(m_previousPresentation);
        m_monitor.reset();
        Animation::mgr()        = std::move(m_previousAnimations);
        Config::animationTree() = std::move(m_previousTree);
        Event::bus()            = std::move(m_previousBus);
        CDMABUFFormatTableTest::TearDown();
    }

    void makeWindow() {
        m_surface           = makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(m_client, 6, m_nextID++));
        m_surface->m_self   = m_surface;
        m_window            = Desktop::View::CWindow::create(makeUnique<Desktop::View::CWaylandBackend>(nullptr));
        m_window->m_monitor = m_monitor;
        m_window->wlSurface()->assign(m_surface, m_window);
    }

    void bindFeedback(uint32_t& id, bool surface = false) {
        id = m_nextID++;
        std::vector<uint32_t> request{2, 0, id};
        if (surface)
            request.push_back(m_surface->id());
        request[1] = (sc<uint32_t>(request.size() * sizeof(uint32_t)) << 16) | (surface ? 3U : 2U);
        ASSERT_EQ(send(m_socket.get(), request.data(), request.size() * sizeof(uint32_t), MSG_NOSIGNAL), sc<ssize_t>(request.size() * sizeof(uint32_t)));
        ASSERT_EQ(wl_event_loop_dispatch(g_pCompositor->m_wlEventLoop, 0), 0);
    }

    void rebuild() {
        PROTO::linuxDma->resetFormatTable();
    }

    void removeMonitor() {
        PROTO::linuxDma->m_formatTable->m_monitorTranches.clear();
        rebuild();
    }

    WP<CSignalListener> configListener(CLinuxDMABufV1Protocol& protocol) {
        return protocol.m_configRefreshed;
    }

    void readFeedback() {
        wl_display_flush_clients(m_display.get());
        std::vector<uint32_t>                       wire;
        std::vector<Hyprutils::OS::CFileDescriptor> fds;
        for (;;) {
            std::array<uint32_t, 4096>                                      data{};
            alignas(cmsghdr) std::array<char, CMSG_SPACE(16 * sizeof(int))> control{};
            iovec                                                           iov{.iov_base = data.data(), .iov_len = sizeof(data)};
            msghdr                                                          msg{.msg_iov = &iov, .msg_iovlen = 1, .msg_control = control.data(), .msg_controllen = control.size()};
            const auto                                                      bytes = recvmsg(m_socket.get(), &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
            if (bytes < 0) {
                ASSERT_EQ(errno, EAGAIN);
                break;
            }
            ASSERT_GT(bytes, 0);
            ASSERT_EQ(msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC), 0);
            ASSERT_EQ(bytes % sizeof(uint32_t), 0U);
            wire.insert(wire.end(), data.begin(), data.begin() + bytes / sizeof(uint32_t));
            for (auto cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
                ASSERT_EQ(cmsg->cmsg_level, SOL_SOCKET);
                ASSERT_EQ(cmsg->cmsg_type, SCM_RIGHTS);
                for (size_t offset = 0; offset < cmsg->cmsg_len - CMSG_LEN(0); offset += sizeof(int)) {
                    int fd = -1;
                    std::memcpy(&fd, CMSG_DATA(cmsg) + offset, sizeof(fd));
                    fds.emplace_back(fd);
                }
            }
        }

        size_t nextFD = 0;
        for (size_t offset = 0; offset < wire.size();) {
            ASSERT_LE(offset + 2, wire.size());
            const auto id     = wire[offset];
            const auto opcode = wire[offset + 1] & 0xFFFF;
            const auto words  = (wire[offset + 1] >> 16) / sizeof(uint32_t);
            ASSERT_GE(words, 2U);
            ASSERT_LE(offset + words, wire.size());
            ASSERT_NE(id, 1U) << "Unexpected wl_display error";
            auto& feedback = m_feedback[id];
            switch (opcode) {
                case 0: // done
                    ++feedback.doneEvents;
                    break;
                case 1: { // format_table: FD is delivered separately via SCM_RIGHTS.
                    ASSERT_EQ(words, 3U);
                    ASSERT_LT(nextFD, fds.size());
                    const auto size = wire[offset + 2];
                    ASSERT_EQ(size % sizeof(SDMABUFFormatTableEntry), 0U);
                    feedback.table.resize(size / sizeof(SDMABUFFormatTableEntry));
                    ASSERT_EQ(pread(fds[nextFD++].get(), feedback.table.data(), size, 0), sc<ssize_t>(size));
                    ++feedback.tableEvents;
                    break;
                }
                case 2: // main_device starts a new set of tranches.
                    feedback.tranches.clear();
                    feedback.pending = {};
                    break;
                case 3: // tranche_done
                    feedback.tranches.push_back(feedback.pending);
                    feedback.pending = {};
                    break;
                case 4: // tranche_target_device
                    break;
                case 5: { // tranche_formats: an array of uint16_t table indices.
                    ASSERT_GE(words, 3U);
                    const auto size = wire[offset + 2];
                    ASSERT_EQ(size % sizeof(uint16_t), 0U);
                    ASSERT_EQ(words, 3 + (size + 3) / sizeof(uint32_t));
                    const auto bytes = rc<const std::byte*>(&wire[offset + 3]);
                    for (size_t i = 0; i < size; i += sizeof(uint16_t)) {
                        uint16_t index = 0;
                        std::memcpy(&index, bytes + i, sizeof(index));
                        ASSERT_LT(index, feedback.table.size());
                        const auto& entry = feedback.table[index];
                        feedback.pending.formats.emplace_back(entry.fmt, entry.modifier);
                    }
                    break;
                }
                case 6: // tranche_flags
                    ASSERT_EQ(words, 3U);
                    feedback.pending.flags = wire[offset + 2];
                    break;
                default: FAIL() << "Unexpected feedback opcode " << opcode;
            }
            offset += words;
        }
        EXPECT_EQ(nextFD, fds.size());
    }

    void expectFeedback(uint32_t id, bool filtered, bool scanout = false) {
        const TFeedbackFormats kms{{DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED}};
        const TFeedbackFormats renderer{
            {DRM_FORMAT_ARGB8888, I915_FORMAT_MOD_X_TILED},
            {DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED},
            {DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR},
        };
        std::vector<SFeedbackTranche> expected;
        if (scanout)
            expected.push_back({ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT, kms});
        expected.push_back({0, filtered ? kms : renderer});
        EXPECT_EQ(m_feedback.at(id).tranches, expected);
    }

    std::map<uint32_t, SFeedbackState> m_feedback;
    PHLMONITOR                         m_monitor;
    PHLWINDOW                          m_window;
    SP<CWLSurfaceResource>             m_surface;

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    Hyprutils::OS::CFileDescriptor                             m_socket;
    wl_client*                                                 m_client = nullptr;
    uint32_t                                                   m_nextID = 3;
    UP<Event::CEventBus>                                       m_previousBus;
    UP<CLinuxDMABufV1Protocol>                                 m_previousProtocol;
    UP<CWLCompositorProtocol>                                  m_previousCompositorProtocol;
    UP<CPresentationProtocol>                                  m_previousPresentation;
    UP<Animation::CHyprAnimationManager>                       m_previousAnimations;
    UP<Config::CAnimationTreeController>                       m_previousTree;
};

TEST_F(CDMABUFFeedbackTest, ExistingAndNewFeedbackUseMatchingTablesWhenShrinkingAndGrowing) {
    uint32_t existing = 0;
    bindFeedback(existing);
    readFeedback();
    expectFeedback(existing, false);
    for (const bool filtered : {true, false}) {
        setFiltering(filtered);
        rebuild();
        readFeedback();
        expectFeedback(existing, filtered);
        uint32_t fresh = 0;
        bindFeedback(fresh);
        readFeedback();
        expectFeedback(fresh, filtered);
        EXPECT_EQ(m_feedback.at(existing).tranches, m_feedback.at(fresh).tranches);
    }
    EXPECT_EQ(m_feedback.at(existing).tableEvents, 3U);
    EXPECT_EQ(m_feedback.at(existing).doneEvents, 3U);
}

TEST_F(CDMABUFFeedbackTest, EverySurfaceFeedbackGetsUpdatedScanoutAndRendererIndices) {
    makeWindow();
    uint32_t first = 0, second = 0;
    bindFeedback(first, true);
    bindFeedback(second, true);
    PROTO::linuxDma->updateScanoutTranche(m_surface, m_monitor);
    readFeedback();
    for (const auto id : {first, second})
        expectFeedback(id, false, true);

    for (const bool filtered : {true, false}) {
        setFiltering(filtered);
        rebuild();
        readFeedback();
        for (const auto id : {first, second})
            expectFeedback(id, filtered, true);
    }
    for (const auto id : {first, second}) {
        EXPECT_EQ(m_feedback.at(id).tableEvents, 3U);
        EXPECT_EQ(m_feedback.at(id).doneEvents, 4U);
    }
}

TEST_F(CDMABUFFeedbackTest, RemovedScanoutTargetGetsCompleteRendererFallback) {
    makeWindow();
    uint32_t id = 0;
    bindFeedback(id, true);
    PROTO::linuxDma->updateScanoutTranche(m_surface, m_monitor);
    readFeedback();
    expectFeedback(id, false, true);
    removeMonitor();
    readFeedback();
    expectFeedback(id, false);
    EXPECT_EQ(m_feedback.at(id).tableEvents, 2U);
    EXPECT_EQ(m_feedback.at(id).doneEvents, 3U);
}

class CDMABUFConfigFeedbackTest : public CDMABUFFeedbackTest {
  protected:
    void SetUp() override {
        CDMABUFFeedbackTest::SetUp();
        m_previousEventLoop                = std::move(g_pEventLoopManager);
        m_previousRefresher                = std::move(Config::Supplementary::refresher());
        g_pEventLoopManager                = makeUnique<CEventLoopManager>(g_pCompositor->m_wlDisplay, g_pCompositor->m_wlEventLoop);
        Config::Supplementary::refresher() = makeUnique<Config::Supplementary::CPropRefresher>();
        m_lua.reset(luaL_newstate());
        ASSERT_TRUE(m_lua);
        Config::Lua::Bindings::registerBindings(m_lua.get(), Config::Lua::mgr().get());
    }

    void TearDown() override {
        m_lua.reset();
        Config::Supplementary::refresher() = std::move(m_previousRefresher);
        g_pEventLoopManager                = std::move(m_previousEventLoop);
        CDMABUFFeedbackTest::TearDown();
    }

    void setRuntimeFiltering(bool enabled) {
        const auto code = std::format("hl.config({{ quirks = {{ skip_non_kms_dmabuf_formats = {} }} }})", enabled ? "true" : "false");
        ASSERT_EQ(luaL_dostring(m_lua.get(), code.c_str()), LUA_OK);
        EXPECT_TRUE(Config::Lua::mgr()->m_errors.empty());
    }

    void dispatchRefresh() {
        ASSERT_EQ(wl_event_loop_dispatch(g_pCompositor->m_wlEventLoop, 0), 0);
        readFeedback();
    }

  private:
    std::unique_ptr<lua_State, decltype(&lua_close)> m_lua{nullptr, lua_close};
    UP<CEventLoopManager>                            m_previousEventLoop;
    UP<Config::Supplementary::CPropRefresher>        m_previousRefresher;
};

TEST_F(CDMABUFConfigFeedbackTest, RuntimeConfigRefreshesBothDirectionsAndSkipsUnchangedValues) {
    uint32_t id = 0;
    bindFeedback(id);
    readFeedback();
    for (const bool filtered : {true, false}) {
        const auto count = m_feedback.at(id).doneEvents;
        setRuntimeFiltering(filtered);
        dispatchRefresh();
        expectFeedback(id, filtered);
        EXPECT_EQ(m_feedback.at(id).doneEvents, count + 1);
        EXPECT_EQ(m_feedback.at(id).tableEvents, count + 1);

        setRuntimeFiltering(filtered);
        dispatchRefresh();
        EXPECT_EQ(m_feedback.at(id).doneEvents, count + 1);
        EXPECT_EQ(m_feedback.at(id).tableEvents, count + 1);

        uint32_t fresh = 0;
        bindFeedback(fresh);
        readFeedback();
        expectFeedback(fresh, filtered);
    }
}

TEST_F(CDMABUFConfigFeedbackTest, FirstReloadPropertyRefreshAppliesChangedSetting) {
    uint32_t id = 0;
    bindFeedback(id);
    readFeedback();
    setFiltering(true);
    // Model reload completion: parsed values, reloaded notification, then the
    // deferred property refresh. Other compositor properties need no refresh here.
    Event::bus()->m_events.config.reloaded.emit();
    Config::Supplementary::refresher()->scheduleRefresh(0);
    dispatchRefresh();
    expectFeedback(id, true);
    EXPECT_EQ(m_feedback.at(id).doneEvents, 2U);
    EXPECT_EQ(m_feedback.at(id).tableEvents, 2U);
}

TEST_F(CDMABUFConfigFeedbackTest, BatchedChangesCompareAgainstAppliedTable) {
    uint32_t id = 0;
    bindFeedback(id);
    readFeedback();
    setRuntimeFiltering(true);
    setRuntimeFiltering(false);
    dispatchRefresh();
    expectFeedback(id, false);
    EXPECT_EQ(m_feedback.at(id).doneEvents, 1U);
    EXPECT_EQ(m_feedback.at(id).tableEvents, 1U);
}

TEST_F(CDMABUFConfigFeedbackTest, MonitorRebuildRecordsSettingBeforeDeferredRefresh) {
    uint32_t id = 0;
    bindFeedback(id);
    readFeedback();
    setRuntimeFiltering(true);
    removeMonitor();
    readFeedback();
    expectFeedback(id, false); // No KMS tranches remain, so retain all renderer formats.
    EXPECT_EQ(m_feedback.at(id).doneEvents, 2U);
    dispatchRefresh();
    EXPECT_EQ(m_feedback.at(id).doneEvents, 2U);
    EXPECT_EQ(m_feedback.at(id).tableEvents, 2U);
}

TEST_F(CDMABUFConfigFeedbackTest, RefreshBeforeInitializationAndAfterDestructionIsSafe) {
    auto       protocol = makeUnique<CLinuxDMABufV1Protocol>(&zwp_linux_dmabuf_v1_interface, 5, "uninitialized-dmabuf-test");
    const auto listener = configListener(*protocol);
    ASSERT_FALSE(listener.expired());
    setRuntimeFiltering(true);
    dispatchRefresh();
    protocol.reset();
    EXPECT_TRUE(listener.expired());
    setRuntimeFiltering(false);
    dispatchRefresh();
}
