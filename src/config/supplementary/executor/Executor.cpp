#include "Executor.hpp"

#include "../../../event/EventBus.hpp"
#include "../../../Compositor.hpp"
#include "../../../managers/input/InputManager.hpp"
#include "../../../desktop/rule/windowRule/WindowRule.hpp"
#include "../../../desktop/rule/Engine.hpp"
#include "../../../desktop/state/FocusState.hpp"
#include "../../../managers/TokenManager.hpp"
#include "../../../helpers/Monitor.hpp"

#include <hyprutils/string/String.hpp>
#include <chrono>

using namespace Config::Supplementary;
using namespace Hyprutils::String;

UP<CExecutor>& Config::Supplementary::executor() {
    static UP<CExecutor> p = makeUnique<CExecutor>();
    return p;
}

CExecutor::CExecutor() {
    m_listeners.init = Event::bus()->m_events.start.listen([this] {
        if (m_firstExecDispatched)
            return;

        // update dbus env
        if (g_pCompositor->m_aqBackend->hasSession())
            spawnRaw(
#ifdef USES_SYSTEMD
                "systemctl --user import-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS && hash "
                "dbus-update-activation-environment 2>/dev/null && "
#endif
                "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS");

        m_firstExecDispatched = true;

        for (auto const& c : m_execOnce) {
            c.withRules ? spawn(c.exec) : spawnRaw(c.exec);
        }

        m_execOnce.clear(); // free some kb of memory :P

        // set input, fixes some certain issues
        g_pInputManager->setKeyboardLayout();
        g_pInputManager->setPointerConfigs();
        g_pInputManager->setTouchDeviceConfigs();
        g_pInputManager->setTabletConfigs();

        // check for user's possible errors with their setup and notify them if needed
        // this is additionally guarded because exiting safe mode will re-run this.
        g_pCompositor->performUserChecks();

        m_listeners.shutdown = Event::bus()->m_events.exit.listen([this] {
            for (auto const& c : m_execShutdown) {
                c.withRules ? spawn(c.exec) : spawnRaw(c.exec);
            }
            m_execShutdown.clear();
        });
    });
}

void CExecutor::addExecOnce(const SExecRequest& cmd) {
    m_execOnce.emplace_back(cmd);
}

void CExecutor::addExecShutdown(const SExecRequest& cmd) {
    m_execShutdown.emplace_back(cmd);
}

std::optional<uint64_t> CExecutor::spawn(const std::string& args) {
    return spawnWithRules(args);
}

std::optional<uint64_t> CExecutor::spawnRaw(const std::string& args) {
    return spawnRawProc(args);
}

std::optional<uint64_t> CExecutor::spawnWithRules(std::string args, PHLWORKSPACE pInitialWorkspace) {
    args = trim(args);

    std::string RULES = "";

    if (args[0] == '[') {
        // we have exec rules
        const auto end = args.find_first_of(']');
        if (end == std::string::npos)
            return std::nullopt;

        RULES = args.substr(1, end - 1);
        args  = args.substr(end + 1);
    }

    std::string execToken = "";

    if (!RULES.empty()) {
        auto       rule = Desktop::Rule::CWindowRule::buildFromExecString(std::move(RULES));

        const auto TOKEN = g_pTokenManager->registerNewToken(nullptr, std::chrono::seconds(1));

        const auto PROC = spawnRawProc(args, pInitialWorkspace, TOKEN);

        if (!PROC)
            return std::nullopt;

        rule->markAsExecRule(TOKEN, *PROC, false /* TODO: could be nice. */);
        rule->registerMatch(Desktop::Rule::RULE_PROP_EXEC_TOKEN, TOKEN);
        rule->registerMatch(Desktop::Rule::RULE_PROP_EXEC_PID, std::to_string(*PROC));
        Desktop::Rule::ruleEngine()->registerRule(std::move(rule));
        Log::logger->log(Log::DEBUG, "Applied rule arguments for exec, pid {}.", *PROC);

        return PROC;
    }

    return spawnRawProc(args, pInitialWorkspace, execToken);
}

static std::vector<std::pair<std::string, std::string>> getHyprlandLaunchEnv(PHLWORKSPACE pInitialWorkspace) {
    static auto PINITIALWSTRACKING = CConfigValue<Config::INTEGER>("misc:initial_workspace_tracking");

    if (!*PINITIALWSTRACKING)
        return {};

    const auto PMONITOR = Desktop::focusState()->monitor();
    if (!PMONITOR || !PMONITOR->m_activeWorkspace)
        return {};

    std::vector<std::pair<std::string, std::string>> result;

    if (!pInitialWorkspace) {
        if (PMONITOR->m_activeSpecialWorkspace)
            pInitialWorkspace = PMONITOR->m_activeSpecialWorkspace;
        else
            pInitialWorkspace = PMONITOR->m_activeWorkspace;
    }

    result.push_back(std::make_pair<>("HL_INITIAL_WORKSPACE_TOKEN",
                                      g_pTokenManager->registerNewToken(Desktop::View::SInitialWorkspaceToken{{}, pInitialWorkspace->getConfigName()}, std::chrono::months(1337))));

    return result;
}

std::optional<uint64_t> CExecutor::spawnRawProc(const std::string& args, PHLWORKSPACE pInitialWorkspace, const std::string& execRuleToken) {
    Log::logger->log(Log::DEBUG, "[executor] Executing {}", args);

    const auto HLENV = getHyprlandLaunchEnv(pInitialWorkspace);

    pid_t      child = fork();
    if (child < 0) {
        Log::logger->log(Log::DEBUG, "Fail to fork");
        return 0;
    }
    if (child == 0) {
        // run in child
        g_pCompositor->restoreNofile();

        sigset_t set;
        sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, nullptr);

        for (auto const& e : HLENV) {
            setenv(e.first.c_str(), e.second.c_str(), 1);
        }
        setenv("WAYLAND_DISPLAY", g_pCompositor->m_wlDisplaySocket.c_str(), 1);
        if (!execRuleToken.empty())
            setenv(Desktop::Rule::EXEC_RULE_ENV_NAME, execRuleToken.c_str(), true);

        int devnull = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);

        // exit child
        _exit(0);
    }
    // run in parent

    Log::logger->log(Log::DEBUG, "[executor] Process created with pid {}", child);

    return child;
}
