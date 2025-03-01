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
        std::println("{}Plugin tests failed, plugin returned:\n{}{}", Colors::RED, Colors::RESET, RESPONSE);
        return false;
    }
    return true;
}
