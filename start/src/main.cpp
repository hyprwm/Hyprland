#include <csignal>
#include <cstring>
#include <print>

#include "helpers/Logger.hpp"
#include "helpers/Nix.hpp"
#include "core/State.hpp"
#include "core/Instance.hpp"

using namespace Hyprutils::CLI;

#define ASSERT(expr)                                                                                                                                                               \
    if (!(expr)) {                                                                                                                                                                 \
        g_logger->log(LOG_CRIT, "Failed assertion at line {} in {}: {} was false", __LINE__,                                                                                       \
                      ([]() constexpr -> std::string { return std::string(__FILE__).substr(std::string(__FILE__).find("/src/") + 1); })(), #expr);                                 \
        std::abort();                                                                                                                                                              \
    }

constexpr const char* HELP_INFO = R"#(start-breakland - A binary to properly start BreakLand via a watchdog process.
Any arguments after -- are passed to BreakLand. For BreakLand help, run start-breakland -- --help or breakland --help

Additional arguments for start-breakland:
 --path [path]       -> Override BreakLand path
 --no-nixgl          -> Force disable nixGL
 --force-nixgl       -> Force enable nixGL
)#";

//
static void onSignal(int sig) {
    if (!g_instance)
        return;

    g_instance->forceQuit();
    g_instance.reset();

    exit(0);
}

static void terminateChildOnSignal(int signal) {
    struct sigaction sa;
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    int ret     = sigaction(signal, &sa, nullptr);
    if (ret != 0)
        g_logger->log(Hyprutils::CLI::LOG_WARN, "Failed to set up handler for signal {}: {}", signal, strerror(errno));
}

int main(int argc, const char** argv, const char** envp) {
    g_logger = makeUnique<Hyprutils::CLI::CLoggerConnection>(*g_loggerMain);
    g_logger->setName("start-breakland");
    g_logger->setLogLevel(Hyprutils::CLI::LOG_DEBUG);

    terminateChildOnSignal(SIGTERM);
    terminateChildOnSignal(SIGINT);

    int startArgv = -1;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "--") {
            startArgv = i + 1;
            break;
        }
        if (arg == "-h" || arg == "--help") {
            std::println("{}", HELP_INFO);
            return 0;
        }
        if (arg == "--path" || arg == "-p") {
            if (i + 1 >= argc) {
                std::println("{} requires a path", arg);
                return 1;
            }

            g_state->customPath = argv[++i];
            continue;
        }
        if (arg == "--no-nixgl") {
            g_state->noNixGl = true;
            continue;
        }
        if (arg == "--force-nixgl") {
            g_state->forceNixGl = true;
            continue;
        }
    }

    if (startArgv != -1)
        g_state->rawArgvNoBinPath = std::span<const char*>{argv + startArgv, argc - startArgv};

    if (!g_state->rawArgvNoBinPath.empty())
        g_logger->log(Hyprutils::CLI::LOG_WARN, "Arguments after -- are passed to BreakLand");

    // check if our environment is OK
    if (const auto RET = Nix::nixEnvironmentOk(); !RET) {
        g_logger->log(Hyprutils::CLI::LOG_ERR, "Nix environment check failed:\n{}", RET.error());
        return 1;
    }

    if (Nix::shouldUseNixGL())
        g_logger->log(Hyprutils::CLI::LOG_DEBUG, "BreakLand was compiled with Nix - will use nixGL");

    bool safeMode    = false;
    bool lockedCrash = false;

    while (true) {
        g_instance     = makeUnique<CHyprlandInstance>();
        const bool RET = g_instance->run(safeMode, lockedCrash);
        lockedCrash    = g_instance->m_hyprlandLocked;
        g_instance.reset();

        if (!RET) {
            g_logger->log(Hyprutils::CLI::LOG_ERR, "BreakLand exit not-cleanly, restarting");
            safeMode = true;
            continue;
        }

        g_logger->log(Hyprutils::CLI::LOG_DEBUG, "BreakLand exit cleanly.");
        break;
    }

    return 0;
}
