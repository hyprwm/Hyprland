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
    const auto RESPONSE = getFromSocket("/dispatch plugin:test:test");

    if (RESPONSE != "ok") {
        NLog::log("{}Plugin tests failed, plugin returned:\n{}{}", Colors::RED, Colors::RESET, RESPONSE);
        return false;
    }
    return true;
}

bool testVkb() {
    const auto RESPONSE = getFromSocket("/dispatch plugin:test:vkb");

    if (RESPONSE != "ok") {
        NLog::log("{}Vkb tests failed, tests returned:\n{}{}", Colors::RED, Colors::RESET, RESPONSE);
        return false;
    }
    return true;
}
