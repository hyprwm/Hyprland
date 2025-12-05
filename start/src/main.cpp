#include <csignal>
#include <print>

#include "helpers/Logger.hpp"
#include "core/State.hpp"
#include "core/Instance.hpp"

using namespace Hyprutils::CLI;

#define ASSERT(expr)                                                                                                                                                               \
    if (!(expr)) {                                                                                                                                                                 \
        g_logger->log(LOG_CRIT, "Failed assertion at line {} in {}: {} was false", __LINE__,                                                                                       \
                      ([]() constexpr -> std::string { return std::string(__FILE__).substr(std::string(__FILE__).find("/src/") + 1); })(), #expr);                                 \
        std::abort();                                                                                                                                                              \
    }

constexpr const char* HELP_INFO = R"#(start-hyprland - A binary to properly start Hyprland via a watchdog process.
Any arguments after -- are passed to Hyprland. For Hyprland help, run start-hyprland -- --help or Hyprland --help)#";

//
static void onSignal(int sig) {
    if (!g_instance)
        return;

    g_instance->forceQuit();
    g_instance.reset();

    exit(0);
}

int main(int argc, const char** argv, const char** envp) {
    g_logger = makeUnique<Hyprutils::CLI::CLoggerConnection>(*g_loggerMain);
    g_logger->setName("start-hyprland");
    g_logger->setLogLevel(Hyprutils::CLI::LOG_DEBUG);

    signal(SIGTERM, ::onSignal);
    signal(SIGINT, ::onSignal);
    signal(SIGKILL, ::onSignal);

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
    }

    if (startArgv != -1)
        g_state->rawArgvNoBinPath = std::span<const char*>{argv + startArgv, argc - startArgv};

    if (!g_state->rawArgvNoBinPath.empty())
        g_logger->log(Hyprutils::CLI::LOG_WARN, "Arguments after -- are passed to Hyprland");

    bool safeMode = false;
    while (true) {
        g_instance     = makeUnique<CHyprlandInstance>();
        const bool RET = g_instance->run(safeMode);
        g_instance.reset();

        if (!RET) {
            g_logger->log(Hyprutils::CLI::LOG_ERR, "Hyprland exit not-cleanly, restarting");
            safeMode = true;
            continue;
        }

        g_logger->log(Hyprutils::CLI::LOG_DEBUG, "Hyprland exit cleanly.");
        break;
    }

    return 0;
}