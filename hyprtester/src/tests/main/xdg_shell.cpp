#include "../../hyprctlCompat.hpp"
#include "../../shared.hpp"
#include "../shared.hpp"
#include "tests.hpp"

#include <chrono>
#include <format>
#include <string>
#include <thread>

static std::string clientBlockForClass(const std::string& className) {
    const auto clients  = getFromSocket("/clients");
    const auto classPos = clients.find(std::format("class: {}", className));
    if (classPos == std::string::npos)
        return "";

    auto blockStart = clients.rfind("Window ", classPos);
    if (blockStart == std::string::npos)
        blockStart = 0;

    auto blockEnd = clients.find("\n\n", classPos);
    if (blockEnd == std::string::npos)
        blockEnd = clients.length();

    return clients.substr(blockStart, blockEnd - blockStart);
}

static std::string waitForClient(const std::string& className) {
    for (int i = 0; i < 50; ++i) {
        if (!clientBlockForClass(className).empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            Tests::sync();
            return clientBlockForClass(className);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return "";
}

SUBTEST(clientStartsNotMaximized, const char* command, const char* className) {
    OK(getFromSocket(std::format("/dispatch hl.dsp.exec_cmd('{}')", command)));

    const auto client = waitForClient(className);
    ASSERT_NOT(client, "");
    EXPECT_CONTAINS(client, std::format("class: {}", className));
    EXPECT_CONTAINS(client, "mapped: 1");
    EXPECT_CONTAINS(client, "xwayland: 0");
    EXPECT_CONTAINS(client, "fullscreen: 0");
    EXPECT_CONTAINS(client, "fullscreenClient: 0");

    OK(getFromSocket(std::format("/dispatch hl.dsp.window.kill({{ window = 'class:{}' }})", className)));
    Tests::waitUntilWindowsN(0);
}

TEST_CASE(xdgDolphinIgnoresMaximizeBeforeInitialCommit) {
    CALL_SUBTEST(clientStartsNotMaximized, "env QT_QPA_PLATFORM=wayland dolphin --new-window", "org.kde.dolphin");
}

TEST_CASE(xdgNautilusIgnoresMaximizeWithoutBuffer) {
    CALL_SUBTEST(clientStartsNotMaximized, "env GDK_BACKEND=wayland nautilus --new-window", "org.gnome.Nautilus");
}
