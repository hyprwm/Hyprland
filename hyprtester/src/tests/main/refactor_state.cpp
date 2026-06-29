#include "../../hyprctlCompat.hpp"
#include "../../shared.hpp"
#include "../shared.hpp"
#include "tests.hpp"

#include <chrono>
#include <format>
#include <string>
#include <thread>

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

static bool waitForMonitorAvailable(const std::string& name) {
    for (int i = 0; i < 50; ++i) {
        if (getFromSocket("/monitors all").contains(name))
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

static bool ensureOutputPresent(const std::string& name) {
    if (getFromSocket("/monitors all").contains(name))
        return true;

    if (getFromSocket(std::format("/output create headless {}", name)) != "ok")
        return false;

    return waitForMonitorAvailable(name);
}

static bool prepareRefactorStateMonitors() {
    if (!ensureOutputPresent("HEADLESS-2") || !ensureOutputPresent("HEADLESS-3"))
        return false;

    if (getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', disabled = false, mode = '1920x1080@60', position = '0x0', scale = '1', transform = 0 })") != "ok")
        return false;

    if (getFromSocket("/eval hl.monitor({ output = 'HEADLESS-3', disabled = false, mode = '1920x1080@60', position = '1920x0', scale = '1', transform = 0 })") != "ok")
        return false;

    Tests::sync();
    return true;
}

TEST_CASE(workspacePlacementActiveMove) {
    CScopeGuard guard = {[&]() {
        Tests::killAllWindows();
        OK(getFromSocket("/reload"));
    }};

    ASSERT(prepareRefactorStateMonitors(), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '270' })"));
    ASSERT(!!Tests::spawnKitty("workspace_move_active"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '271' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '270' })"));

    OK(getFromSocket("/dispatch hl.dsp.workspace.move({ workspace = '270', monitor = 'HEADLESS-3' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    {
        const auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 270");
        EXPECT_CONTAINS(str, "HEADLESS-3");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    {
        const auto str = getFromSocket("/activeworkspace");
        EXPECT_NOT_CONTAINS(str, "workspace ID 270");
        EXPECT_CONTAINS(str, "HEADLESS-2");
    }
}

TEST_CASE(workspacePlacementWindowState) {
    CScopeGuard guard = {[&]() {
        Tests::killAllWindows();
        OK(getFromSocket("/reload"));
    }};

    ASSERT(prepareRefactorStateMonitors(), true);
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '280' })"));

    ASSERT(!!Tests::spawnKitty("workspace_move_pinned"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.pin()"));

    ASSERT(!!Tests::spawnKitty("workspace_move_float"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 400, y = 300 })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 200, y = 200 })"));

    ASSERT(!!Tests::spawnKitty("workspace_move_fullscreen"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    OK(getFromSocket("/dispatch hl.dsp.workspace.move({ workspace = '280', monitor = 'HEADLESS-3' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:workspace_move_pinned' })"));
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), "workspace: 280");

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:workspace_move_float' })"));
    {
        const auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: workspace_move_float");
        EXPECT_CONTAINS(str, "at: 2120,200");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:workspace_move_fullscreen' })"));
    {
        const auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: workspace_move_fullscreen");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "at: 1920,0");
        EXPECT_CONTAINS(str, "size: 1920,1080");
    }
}

TEST_CASE(globalWindowMoveState) {
    CScopeGuard guard = {[&]() {
        Tests::killAllWindows();
        OK(getFromSocket("/reload"));
    }};

    ASSERT(prepareRefactorStateMonitors(), true);
    OK(getFromSocket("/eval hl.config({ group = { group_on_movetoworkspace = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '291' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '290' })"));
    ASSERT(!!Tests::spawnKitty("global_move_fullscreen"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = '291', follow = false })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '291' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:global_move_fullscreen' })"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 2");

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '293' })"));
    ASSERT(!!Tests::spawnKitty("global_move_group_anchor"), true);
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '292' })"));
    ASSERT(!!Tests::spawnKitty("global_move_float"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 400, y = 300 })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 200, y = 200 })"));

    OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = '293', follow = true })"));
    {
        const auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: global_move_float");
        EXPECT_CONTAINS(str, "at: 2120,200");
    }

    OK(getFromSocket("/dispatch hl.dsp.group.next()"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: global_move_group_anchor");
}

TEST_CASE(pointerNoWarpsFocusesMonitorOnly) {
    CScopeGuard guard = {[&]() {
        OK(getFromSocket("/eval hl.config({ cursor = { no_warps = false } })"));
        Tests::killAllWindows();
        OK(getFromSocket("/reload"));
    }};

    ASSERT(prepareRefactorStateMonitors(), true);
    Tests::killAllWindows();

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '301' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '300' })"));

    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 100, y = 100 })"));
    const auto originalCursor = getFromSocket("/cursorpos");
    OK(getFromSocket("/eval hl.config({ cursor = { no_warps = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    EXPECT(getFromSocket("/cursorpos"), originalCursor);
    {
        const auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 301");
        EXPECT_CONTAINS(str, "HEADLESS-3");
    }

    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 2000, y = 10 })"));
    EXPECT_NOT(getFromSocket("/cursorpos"), originalCursor);
}
