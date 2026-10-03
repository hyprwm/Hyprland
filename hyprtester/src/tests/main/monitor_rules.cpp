#include "../../hyprctlCompat.hpp"
#include "../../shared.hpp"
#include "../shared.hpp"
#include "tests.hpp"

#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <thread>

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

static constexpr const char* TEST_MIRROR_SOURCE = "HYPRTEST-MIRROR-SOURCE";
static constexpr const char* TEST_MIRROR_TARGET = "HYPRTEST-MIRROR-TARGET";

static std::string           monitorBlock(const std::string& response, const std::string& name) {
    const auto HEADER = std::format("Monitor {} (ID ", name);
    const auto BEGIN  = response.find(HEADER);

    if (BEGIN == std::string::npos)
        return "";

    const auto END = response.find("\n\n", BEGIN);
    return response.substr(BEGIN, END == std::string::npos ? std::string::npos : END - BEGIN);
}

static std::string monitorBlock(const std::string& name, bool all = true) {
    return monitorBlock(getFromSocket(all ? "/monitors all" : "/monitors"), name);
}

static std::optional<std::string> monitorID(const std::string& name) {
    const auto BLOCK = monitorBlock(name);
    const auto BEGIN = BLOCK.find("(ID ");

    if (BEGIN == std::string::npos)
        return std::nullopt;

    const auto END = BLOCK.find("):", BEGIN);
    if (END == std::string::npos)
        return std::nullopt;

    return BLOCK.substr(BEGIN + 4, END - BEGIN - 4);
}

static bool waitForMonitor(const std::string& name, bool present, bool all = true) {
    for (int i = 0; i < 50; ++i) {
        const bool IS_PRESENT = !monitorBlock(name, all).empty();
        if (IS_PRESENT == present)
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

static bool waitForMirror(const std::string& source, const std::string& targetID) {
    const auto MIRROR = std::format("mirrorOf: {}", targetID);

    for (int i = 0; i < 50; ++i) {
        if (monitorBlock(source).contains(MIRROR))
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

static std::string expectMonitorRenderStages(const std::string& name, bool mirror, std::optional<bool> workspaceProbe = std::nullopt) {
    CScopeGuard resetRecording([] { getFromSocket("/eval hl.plugin.test.reset_monitor_render_recording()"); });
    const auto  ARMED =
        getFromSocket(std::format("/eval hl.plugin.test.arm_monitor_render_recording('{}'{})", name, workspaceProbe ? (*workspaceProbe ? ", true" : ", false") : ""));
    if (ARMED != "ok")
        return ARMED;

    std::string result;
    for (int i = 0; i < 50; ++i) {
        result = getFromSocket(std::format("/eval hl.plugin.test.check_monitor_render_recording({})", mirror ? "true" : "false"));
        if (!result.contains("Monitor render pending:"))
            return result;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return std::format("Timed out waiting for {} frame on '{}': {}", mirror ? "mirror" : "normal", name, result);
}

static void removeMirrorTestOutputs() {
    if (!monitorBlock(TEST_MIRROR_TARGET, false).empty()) {
        getFromSocket(std::format("/output remove {}", TEST_MIRROR_TARGET));
        waitForMonitor(TEST_MIRROR_TARGET, false);
        waitForMonitor(TEST_MIRROR_SOURCE, true, false);
    }

    if (!monitorBlock(TEST_MIRROR_SOURCE, false).empty()) {
        getFromSocket(std::format("/output remove {}", TEST_MIRROR_SOURCE));
        waitForMonitor(TEST_MIRROR_SOURCE, false);
    }

    getFromSocket("/reload");
}

TEST_CASE(monitorMirrorAppliedWhenTargetAppears) {
    removeMirrorTestOutputs();
    CScopeGuard guard = {[]() { removeMirrorTestOutputs(); }};

    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '1920x1080@60', position = 'auto-right', scale = '1', mirror = '{}' }}); "
                                 "hl.monitor({{ output = '{}', mode = '1920x1080@60', position = 'auto-right', scale = '1' }})",
                                 TEST_MIRROR_SOURCE, TEST_MIRROR_TARGET, TEST_MIRROR_TARGET)));

    OK(getFromSocket(std::format("/output create headless {}", TEST_MIRROR_SOURCE)));
    ASSERT(waitForMonitor(TEST_MIRROR_SOURCE, true), true);
    EXPECT_CONTAINS(monitorBlock(TEST_MIRROR_SOURCE), "mirrorOf: none");
    EXPECT(monitorBlock(TEST_MIRROR_SOURCE, false).empty(), false);
    OK(expectMonitorRenderStages(TEST_MIRROR_SOURCE, false));

    OK(getFromSocket(std::format("/output create headless {}", TEST_MIRROR_TARGET)));
    ASSERT(waitForMonitor(TEST_MIRROR_TARGET, true), true);

    const auto TARGET_ID = monitorID(TEST_MIRROR_TARGET);
    ASSERT(TARGET_ID.has_value(), true);
    ASSERT(waitForMirror(TEST_MIRROR_SOURCE, *TARGET_ID), true);
    EXPECT(monitorBlock(TEST_MIRROR_SOURCE, false).empty(), true);
    EXPECT(monitorBlock(TEST_MIRROR_TARGET, false).empty(), false);
    OK(expectMonitorRenderStages(TEST_MIRROR_SOURCE, true));
    OK(expectMonitorRenderStages(TEST_MIRROR_TARGET, false));
    // Exercise resource isolation with an existing monitor-owned mirror cache.
    OK(getFromSocket("/eval hl.config({ misc = { disable_hyprland_logo = true, disable_splash_rendering = true } })"));
    OK(expectMonitorRenderStages(TEST_MIRROR_TARGET, false, false));
    ASSERT_CONTAINS(getFromSocket("/version"), "Hyprland");

    OK(getFromSocket(std::format("/output remove {}", TEST_MIRROR_TARGET)));
    ASSERT(waitForMonitor(TEST_MIRROR_TARGET, false), true);
    ASSERT(waitForMonitor(TEST_MIRROR_SOURCE, true, false), true);
    EXPECT_CONTAINS(monitorBlock(TEST_MIRROR_SOURCE), "mirrorOf: none");
    OK(expectMonitorRenderStages(TEST_MIRROR_SOURCE, false));

    OK(getFromSocket(std::format("/output create headless {}", TEST_MIRROR_TARGET)));
    ASSERT(waitForMonitor(TEST_MIRROR_TARGET, true), true);

    const auto RECREATED_TARGET_ID = monitorID(TEST_MIRROR_TARGET);
    ASSERT(RECREATED_TARGET_ID.has_value(), true);
    ASSERT(waitForMirror(TEST_MIRROR_SOURCE, *RECREATED_TARGET_ID), true);
    EXPECT(monitorBlock(TEST_MIRROR_SOURCE, false).empty(), true);
    OK(expectMonitorRenderStages(TEST_MIRROR_SOURCE, true));
}

TEST_CASE(monitorBackgroundWorkspaceAndXpMode) {
    removeMirrorTestOutputs();
    CScopeGuard cleanup([] { removeMirrorTestOutputs(); });
    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '1920x1080@60', position = 'auto-right', scale = '1' }}); "
                                 "hl.config({{ misc = {{ disable_hyprland_logo = true, disable_splash_rendering = true }} }})",
                                 TEST_MIRROR_SOURCE)));
    OK(getFromSocket(std::format("/output create headless {}", TEST_MIRROR_SOURCE)));
    ASSERT(waitForMonitor(TEST_MIRROR_SOURCE, true, false), true);
    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ monitor = '{}' }})", TEST_MIRROR_SOURCE)));
    for (const bool XP_MODE : {false, true}) {
        OK(getFromSocket(std::format("/eval hl.config({{ render = {{ xp_mode = {} }} }})", XP_MODE ? "true" : "false")));
        for (const bool WITH_WORKSPACE : {true, false}) {
            NLog::log("Checking background: workspace {}, xp_mode {}", WITH_WORKSPACE, XP_MODE);
            EXPECT_OK(expectMonitorRenderStages(TEST_MIRROR_SOURCE, false, WITH_WORKSPACE));
        }
    }
}
