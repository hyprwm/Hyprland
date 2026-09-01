#include "../../hyprctlCompat.hpp"
#include "../../shared.hpp"
#include "../shared.hpp"
#include "build.hpp"
#include "tests.hpp"

#include <chrono>
#include <cstddef>
#include <csignal>
#include <exception>
#include <format>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <hyprutils/os/Process.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

class CInitialMaximizeClient {
  public:
    CInitialMaximizeClient(const std::string& timing) {
        m_process = makeUnique<CProcess>(std::format("{}/xdg-initial-maximize", binaryDir), std::vector<std::string>{timing});
        m_process->addEnv("WAYLAND_DISPLAY", WLDISPLAY);
        if (!m_process->runAsync())
            throw std::exception();

        for (size_t i = 0; i < 50; ++i) {
            if (!Tests::processAlive(m_process->pid()))
                throw std::exception();

            const auto CLIENTS = getFromSocket("/clients");
            if (Tests::windowCount() == 1 && CLIENTS.contains("class: xdg-initial-maximize") && CLIENTS.contains("mapped: 1"))
                return;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        kill(m_process->pid(), SIGKILL);
        throw std::exception();
    }

    ~CInitialMaximizeClient() {
        if (m_process && Tests::processAlive(m_process->pid()))
            kill(m_process->pid(), SIGKILL);
    }

  private:
    CUniquePointer<CProcess> m_process;
};

SUBTEST(initialMaximizeIsIgnored, const char* timing) {
    std::optional<CInitialMaximizeClient> client;
    try {
        client.emplace(timing);
    } catch (...) { FAIL_TEST("Failed to start xdg-initial-maximize client in mode {}", timing); }

    const auto CLIENT = getFromSocket("/clients");
    EXPECT_CONTAINS(CLIENT, "class: xdg-initial-maximize");
    EXPECT_CONTAINS(CLIENT, "mapped: 1");
    EXPECT_CONTAINS(CLIENT, "xwayland: 0");
    EXPECT_CONTAINS(CLIENT, "fullscreen: 0");
    EXPECT_CONTAINS(CLIENT, "fullscreenClient: 0");
}

TEST_CASE(xdgMaximizeBeforeInitialCommitIsIgnored) {
    CALL_SUBTEST(initialMaximizeIsIgnored, "before-initial-commit");
}

TEST_CASE(xdgMaximizeBeforeMapIsIgnored) {
    CALL_SUBTEST(initialMaximizeIsIgnored, "before-map");
}
