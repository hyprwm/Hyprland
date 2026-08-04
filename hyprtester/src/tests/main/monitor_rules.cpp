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

    OK(getFromSocket(std::format("/output create headless {}", TEST_MIRROR_TARGET)));
    ASSERT(waitForMonitor(TEST_MIRROR_TARGET, true), true);

    const auto TARGET_ID = monitorID(TEST_MIRROR_TARGET);
    ASSERT(TARGET_ID.has_value(), true);
    ASSERT(waitForMirror(TEST_MIRROR_SOURCE, *TARGET_ID), true);
    EXPECT(monitorBlock(TEST_MIRROR_SOURCE, false).empty(), true);
    EXPECT(monitorBlock(TEST_MIRROR_TARGET, false).empty(), false);
    ASSERT_CONTAINS(getFromSocket("/version"), "Hyprland");

    OK(getFromSocket(std::format("/output remove {}", TEST_MIRROR_TARGET)));
    ASSERT(waitForMonitor(TEST_MIRROR_TARGET, false), true);
    ASSERT(waitForMonitor(TEST_MIRROR_SOURCE, true, false), true);
    EXPECT_CONTAINS(monitorBlock(TEST_MIRROR_SOURCE), "mirrorOf: none");

    OK(getFromSocket(std::format("/output create headless {}", TEST_MIRROR_TARGET)));
    ASSERT(waitForMonitor(TEST_MIRROR_TARGET, true), true);

    const auto RECREATED_TARGET_ID = monitorID(TEST_MIRROR_TARGET);
    ASSERT(RECREATED_TARGET_ID.has_value(), true);
    ASSERT(waitForMirror(TEST_MIRROR_SOURCE, *RECREATED_TARGET_ID), true);
    EXPECT(monitorBlock(TEST_MIRROR_SOURCE, false).empty(), true);
}
