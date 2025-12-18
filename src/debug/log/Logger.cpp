#include "Logger.hpp"
#include "RollingLogFollow.hpp"

#include "../../defines.hpp"

#include "../../managers/HookSystemManager.hpp"
#include "../../config/ConfigValue.hpp"

using namespace Log;

CLogger::CLogger() {
    const auto IS_TRACE = Env::isTrace();
    m_logger.setLogLevel(IS_TRACE ? Hyprutils::CLI::LOG_TRACE : Hyprutils::CLI::LOG_DEBUG);
}

void CLogger::log(Hyprutils::CLI::eLogLevel level, const std::string_view& str) {

    static bool TRACE = Env::isTrace();

    if (!m_logsEnabled)
        return;

    if (level == Hyprutils::CLI::LOG_TRACE && !TRACE)
        return;

    if (SRollingLogFollow::get().isRunning())
        SRollingLogFollow::get().addLog(str);

    m_logger.log(level, str);
}

void CLogger::initIS(const std::string_view& IS) {
    // NOLINTNEXTLINE
    m_logger.setOutputFile(std::string{IS} + (ISDEBUG ? "/hyprlandd.log" : "/hyprland.log"));
    m_logger.setEnableRolling(true);
    m_logger.setEnableColor(false);
    m_logger.setEnableStdout(true);
    m_logger.setTime(false);
}

void CLogger::initCallbacks() {
    static auto P = g_pHookSystem->hookDynamic("configReloaded", [this](void* hk, SCallbackInfo& info, std::any param) { recheckCfg(); });
    recheckCfg();
}

void CLogger::recheckCfg() {
    static auto PDISABLELOGS  = CConfigValue<Hyprlang::INT>("debug:disable_logs");
    static auto PDISABLETIME  = CConfigValue<Hyprlang::INT>("debug:disable_time");
    static auto PENABLESTDOUT = CConfigValue<Hyprlang::INT>("debug:enable_stdout_logs");
    static auto PENABLECOLOR  = CConfigValue<Hyprlang::INT>("debug:colored_stdout_logs");

    m_logger.setEnableStdout(!*PDISABLELOGS && *PENABLESTDOUT);
    m_logsEnabled = !*PDISABLELOGS;
    m_logger.setTime(!*PDISABLETIME);
    m_logger.setEnableColor(*PENABLECOLOR);
}

const std::string& CLogger::rolling() {
    return m_logger.rollingLog();
}

Hyprutils::CLI::CLogger& CLogger::hu() {
    return m_logger;
}
