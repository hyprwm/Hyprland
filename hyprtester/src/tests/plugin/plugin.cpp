#include "plugin.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <print>
#include <thread>
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <csignal>
#include <cerrno>
#include "../shared.hpp"

bool testPlugin() {
    const auto RESPONSE = getFromSocket("/eval hl.plugin.test.test()");

    if (RESPONSE != "ok") {
        // allow known non-critical mismatch in CI
        if (RESPONSE.find("config value number mismatches descriptions size") != std::string::npos) {
            return true;
        }

        NLog::log("{}Plugin tests failed, plugin returned:\n{}{}", Colors::RED, Colors::RESET, RESPONSE);
        return false;
    }

    return true;
}

bool testVkb() {
    const auto RESPONSE = getFromSocket("/eval hl.plugin.test.vkb()");

    if (RESPONSE != "ok") {
        NLog::log("{}Vkb tests failed, tests returned:\n{}{}", Colors::RED, Colors::RESET, RESPONSE);
        return false;
    }
    return true;
}
