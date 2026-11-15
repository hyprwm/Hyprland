#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "build.hpp"
#include "tests.hpp"

#include <array>
#include <chrono>
#include <csignal>
#include <thread>
#include <sys/poll.h>
#include <unistd.h>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

class CFractionalClient {
  public:
    CFractionalClient() : m_process(std::format("{}/fractional-subsurface", binaryDir), {}) {
        ;
    }
    ~CFractionalClient() {
        if (m_process.pid() > 0 && Tests::processAlive(m_process.pid()))
            kill(m_process.pid(), SIGKILL);
    }

    bool start() {
        std::array<int, 2> input = {-1, -1}, output = {-1, -1};
        if (pipe(input.data()) != 0)
            return false;
        const CFileDescriptor childInput{input[0]};
        m_input = CFileDescriptor{input[1]};
        if (pipe(output.data()) != 0)
            return false;
        m_output = CFileDescriptor{output[0]};
        const CFileDescriptor childOutput{output[1]};
        m_process.setStdinFD(childInput.get());
        m_process.setStdoutFD(childOutput.get());
        m_process.addEnv("WAYLAND_DISPLAY", WLDISPLAY);
        return m_process.runAsync() && response() == "ready";
    }

    bool command(const std::string& command) {
        const auto LINE = command + '\n';
        return write(m_input.get(), LINE.data(), LINE.size()) == sc<ssize_t>(LINE.size()) && response() == "ok";
    }

  private:
    std::string response() {
        std::string line;
        const auto  DEADLINE = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < DEADLINE) {
            pollfd fd = {.fd = m_output.get(), .events = POLLIN};
            if (poll(&fd, 1, 100) <= 0)
                continue;
            char byte = 0;
            if (read(m_output.get(), &byte, 1) != 1)
                break;
            if (byte == '\n')
                return line;
            line += byte;
        }
        return line;
    }

    CProcess        m_process;
    CFileDescriptor m_input, m_output;
};

static std::string probeEdges(bool red) {
    std::string result;
    for (int i = 0; i < 100; ++i) {
        result = getFromSocket(std::format("/eval hl.plugin.test.arm_subsurface_edges({})", red ? "true" : "false"));
        if (result == "ok")
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (result != "ok")
        return result;
    for (int i = 0; i < 100; ++i) {
        result = getFromSocket("/eval hl.plugin.test.check_subsurface_edges()");
        if (!result.contains("Edge probe pending"))
            return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return result;
}

TEST_CASE(fractionalTiledSubsurfaceEdges) {
    const CScopeGuard cleanup([] {
        Tests::killAllWindows();
        getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = 'auto-right', scale = '1' })");
    });
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '2880x1920@60', position = '0x0', scale = '1.33333' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '261' })"));
    OK(getFromSocket("/eval hl.config({ general = { layout = 'dwindle', border_size = 1, gaps_in = -0.5, gaps_out = 0, col = { active_border = 'rgba(0000ffff)', inactive_border = "
                     "'rgba(0000ffff)' } }, decoration = { rounding = 0, blur = { enabled = false }, shadow = { enabled = false } }, dwindle = { force_split = 2, preserve_split = "
                     "true }, misc = { animate_manual_resizes = false } })"));

    CFractionalClient client;
    ASSERT(client.start(), true);
    ASSERT(client.command("sync"), true);
    // Exercise the full-width window, then retained edges on frames without new client commits.
    for (int i = 0; i < 4; ++i)
        EXPECT_OK(probeEdges(false));

    SPAWN_KITTY("fractional-neighbor");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:fractional-subsurface' })"));
    for (const auto* action : {"splitratio 1 exact", "swapsplit", "swapsplit", "togglesplit", "swapsplit", "splitratio 1.2 exact", "splitratio 1 exact"}) {
        NLog::log("Checking fractional subsurface after {}", action);
        OK(getFromSocket(std::format("/dispatch hl.dsp.layout('{}')", action)));
        ASSERT(client.command("sync"), true);
        EXPECT_OK(probeEdges(false));
        ASSERT(client.command("edges"), true);
        // Keep checking the partial update after its damage has left the current frame.
        for (int i = 0; i < 4; ++i)
            EXPECT_OK(probeEdges(true));
        ASSERT(client.command("edges"), true);
        EXPECT_OK(probeEdges(false));
    }

    OK(getFromSocket("/eval hl.config({ animations = { enabled = true }, misc = { animate_manual_resizes = true } })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('splitratio 1.1 exact')"));
    ASSERT(client.command("sync"), true);
    EXPECT_OK(probeEdges(false)); // Wait for the animation to settle before sampling.
    ASSERT(client.command("edges"), true);
    EXPECT_OK(probeEdges(true));
}
