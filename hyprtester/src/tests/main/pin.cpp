#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include <thread>
#include <chrono>

static int ret = 0;

static bool test() {
    NLog::log("{}Testing scoped pin", Colors::GREEN);

    getFromSocket("/dispatch workspace 1"); // no OK: we might already be on 1
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    // set up rules: float + pin to workspaces 2-3
    OK(getFromSocket("/keyword windowrulev2 float, class:pintest"));
    OK(getFromSocket("/keyword windowrulev2 pin 2-3, class:pintest"));

    // spawn the window on workspace 2
    NLog::log("{}Switching to workspace 2 and spawning pintest", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 2"));
    auto kittyProc = Tests::spawnKitty("pintest");
    if (!kittyProc) {
        NLog::log("{}Error: pintest kitty did not spawn", Colors::RED);
        return false;
    }

    // verify window is pinned with scoped workspaces
    NLog::log("{}Verifying pinned state on workspace 2", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "class: pintest");
    }

    // verify active window is pintest on workspace 2
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: pintest");
        EXPECT_CONTAINS(str, "workspace: 2 (2)");
    }

    // switch to workspace 3 (in the pin set) -- window should follow
    NLog::log("{}Switching to workspace 3 (in pin set), window should follow", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 3"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: pintest");
        // window should now be on workspace 3
        EXPECT_CONTAINS(str, "workspace: 3 (3)");
    }

    // switch to workspace 1 (NOT in the pin set) -- window should stay on 3
    NLog::log("{}Switching to workspace 1 (not in pin set), window should hide", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        // pintest should still exist but be on workspace 3, not workspace 1
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: pintest");
        EXPECT_NOT_CONTAINS(str, "workspace: 1 (1)\n\tfloating: 1\n\tpseudo: 0\n\tmonitor: 0\n\tclass: pintest");
    }

    // switch back to workspace 2 (in pin set) -- window should reappear from workspace 3
    NLog::log("{}Switching to workspace 2 (in pin set), window should reappear", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 2"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: pintest");
        // window should have moved to workspace 2
        EXPECT_CONTAINS(str, "workspace: 2 (2)");
    }

    // test global pin: spawn a globally pinned window and verify it follows everywhere
    NLog::log("{}Testing global pin still works", Colors::YELLOW);
    OK(getFromSocket("/keyword windowrulev2 float, class:globalpin"));
    OK(getFromSocket("/keyword windowrulev2 pin, class:globalpin"));

    auto kittyGlobal = Tests::spawnKitty("globalpin");
    if (!kittyGlobal) {
        NLog::log("{}Error: globalpin kitty did not spawn", Colors::RED);
        return false;
    }

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: globalpin");
        EXPECT_CONTAINS(str, "pinned: 1");
    }

    // switch to workspace 5 -- globally pinned window should follow
    NLog::log("{}Switching to workspace 5, global pin should follow", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 5"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/clients");
        // globalpin should be on workspace 5 now
        // find the globalpin entry and check its workspace
        auto pos = str.find("class: globalpin");
        EXPECT(pos != std::string::npos, true);
        if (pos != std::string::npos) {
            auto before = str.substr(0, pos);
            auto lastWs = before.rfind("workspace: ");
            EXPECT(lastWs != std::string::npos, true);
            if (lastWs != std::string::npos) {
                EXPECT_CONTAINS(before.substr(lastWs), "workspace: 5 (5)");
            }
        }
    }

    // switch back to workspace 1 -- global pin should follow, scoped pin should not
    NLog::log("{}Switching to workspace 1, verifying both pin types", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/clients");

        // globalpin should be on workspace 1
        auto gpos = str.find("class: globalpin");
        EXPECT(gpos != std::string::npos, true);
        if (gpos != std::string::npos) {
            auto before = str.substr(0, gpos);
            auto lastWs = before.rfind("workspace: ");
            if (lastWs != std::string::npos) {
                EXPECT_CONTAINS(before.substr(lastWs), "workspace: 1 (1)");
            }
        }

        // pintest should NOT be on workspace 1
        auto ppos = str.find("class: pintest");
        EXPECT(ppos != std::string::npos, true);
        if (ppos != std::string::npos) {
            auto before = str.substr(0, ppos);
            auto lastWs = before.rfind("workspace: ");
            if (lastWs != std::string::npos) {
                EXPECT_NOT_CONTAINS(before.substr(lastWs), "workspace: 1 (1)");
            }
        }
    }

    // test pin toggle via dispatcher clears scoped state
    NLog::log("{}Testing pin toggle clears scoped state", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 2"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    OK(getFromSocket("/dispatch focuswindow class:pintest"));
    OK(getFromSocket("/dispatch pin"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: pintest");
        EXPECT_CONTAINS(str, "pinned: 0");
    }

    // re-pin via dispatcher should give global pin, not restore scoped state
    NLog::log("{}Testing re-pin via dispatcher is global", Colors::YELLOW);
    OK(getFromSocket("/dispatch pin"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinnedWorkspaces: \n");
    }
    OK(getFromSocket("/dispatch pin"));

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    // test individual workspace IDs (not a range)
    NLog::log("{}Testing individual workspace IDs in pin rule", Colors::YELLOW);
    OK(getFromSocket("/keyword windowrulev2 float, class:pinids"));
    OK(getFromSocket("/keyword windowrulev2 pin 2 5 8, class:pinids"));
    getFromSocket("/dispatch workspace 2"); // no OK: previous workspace may not exist after cleanup
    auto kittyIds = Tests::spawnKitty("pinids");
    if (!kittyIds) {
        NLog::log("{}Error: pinids kitty did not spawn", Colors::RED);
        return false;
    }
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: pinids");
        EXPECT_CONTAINS(str, "pinnedWorkspaces: 2 5 8");
    }

    // should follow to workspace 5 (in set)
    OK(getFromSocket("/dispatch workspace 5"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/clients");
        auto pos = str.find("class: pinids");
        EXPECT(pos != std::string::npos, true);
        if (pos != std::string::npos) {
            auto before = str.substr(0, pos);
            auto lastWs = before.rfind("workspace: ");
            if (lastWs != std::string::npos)
                EXPECT_CONTAINS(before.substr(lastWs), "workspace: 5 (5)");
        }
    }

    // should NOT follow to workspace 4 (not in set)
    OK(getFromSocket("/dispatch workspace 4"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/clients");
        auto pos = str.find("class: pinids");
        EXPECT(pos != std::string::npos, true);
        if (pos != std::string::npos) {
            auto before = str.substr(0, pos);
            auto lastWs = before.rfind("workspace: ");
            if (lastWs != std::string::npos)
                EXPECT_NOT_CONTAINS(before.substr(lastWs), "workspace: 4 (4)");
        }
    }

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    // test two scoped-pinned windows with different sets
    NLog::log("{}Testing multiple scoped pins with different sets", Colors::YELLOW);
    OK(getFromSocket("/keyword windowrulev2 float, class:pinA"));
    OK(getFromSocket("/keyword windowrulev2 pin 1-2, class:pinA"));
    OK(getFromSocket("/keyword windowrulev2 float, class:pinB"));
    OK(getFromSocket("/keyword windowrulev2 pin 3-4, class:pinB"));

    OK(getFromSocket("/dispatch workspace 1"));
    auto kittyA = Tests::spawnKitty("pinA");
    if (!kittyA) {
        NLog::log("{}Error: pinA kitty did not spawn", Colors::RED);
        return false;
    }

    OK(getFromSocket("/dispatch workspace 3"));
    auto kittyB = Tests::spawnKitty("pinB");
    if (!kittyB) {
        NLog::log("{}Error: pinB kitty did not spawn", Colors::RED);
        return false;
    }

    // switch to workspace 2: pinA follows, pinB stays on 3
    OK(getFromSocket("/dispatch workspace 2"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/clients");

        auto posA = str.find("class: pinA");
        EXPECT(posA != std::string::npos, true);
        if (posA != std::string::npos) {
            auto before = str.substr(0, posA);
            auto lastWs = before.rfind("workspace: ");
            if (lastWs != std::string::npos)
                EXPECT_CONTAINS(before.substr(lastWs), "workspace: 2 (2)");
        }

        auto posB = str.find("class: pinB");
        EXPECT(posB != std::string::npos, true);
        if (posB != std::string::npos) {
            auto before = str.substr(0, posB);
            auto lastWs = before.rfind("workspace: ");
            if (lastWs != std::string::npos)
                EXPECT_NOT_CONTAINS(before.substr(lastWs), "workspace: 2 (2)");
        }
    }

    // switch to workspace 4: pinB follows, pinA stays on 2
    OK(getFromSocket("/dispatch workspace 4"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto str = getFromSocket("/clients");

        auto posB = str.find("class: pinB");
        EXPECT(posB != std::string::npos, true);
        if (posB != std::string::npos) {
            auto before = str.substr(0, posB);
            auto lastWs = before.rfind("workspace: ");
            if (lastWs != std::string::npos)
                EXPECT_CONTAINS(before.substr(lastWs), "workspace: 4 (4)");
        }

        auto posA = str.find("class: pinA");
        EXPECT(posA != std::string::npos, true);
        if (posA != std::string::npos) {
            auto before = str.substr(0, posA);
            auto lastWs = before.rfind("workspace: ");
            if (lastWs != std::string::npos)
                EXPECT_NOT_CONTAINS(before.substr(lastWs), "workspace: 4 (4)");
        }
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
