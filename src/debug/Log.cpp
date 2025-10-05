#include "Log.hpp"
#include "../defines.hpp"
#include "RollingLogFollow.hpp"

#include <fstream>
#include <print>
#include <fcntl.h>

void Debug::init(const std::string& IS) {
    m_logFile = IS + (ISDEBUG ? "/hyprlandd.log" : "/hyprland.log");
    m_logOfs.open(m_logFile, std::ios::out | std::ios::app);
    auto handle = m_logOfs.native_handle();
    fcntl(handle, F_SETFD, FD_CLOEXEC);
}

void Debug::close() {
    m_logOfs.close();
}

void Debug::log(eLogLevel level, std::string str) {
    if (level == TRACE && !m_trace)
        return;

    if (m_shuttingDown)
        return;

    std::lock_guard<std::mutex> guard(m_logMutex);

    std::string                 coloredStr = str;
    //NOLINTBEGIN
    switch (level) {
        case LOG:
            str        = "[LOG] " + str;
            coloredStr = str;
            break;
        case WARN:
            str        = "[WARN] " + str;
            coloredStr = "\033[1;33m" + str + "\033[0m"; // yellow
            break;
        case ERR:
            str        = "[ERR] " + str;
            coloredStr = "\033[1;31m" + str + "\033[0m"; // red
            break;
        case CRIT:
            str        = "[CRITICAL] " + str;
            coloredStr = "\033[1;35m" + str + "\033[0m"; // magenta
            break;
        case INFO:
            str        = "[INFO] " + str;
            coloredStr = "\033[1;32m" + str + "\033[0m"; // green
            break;
        case TRACE:
            str        = "[TRACE] " + str;
            coloredStr = "\033[1;34m" + str + "\033[0m"; // blue
            break;
        default: break;
    }
    //NOLINTEND

    m_rollingLog += str + "\n";
    if (m_rollingLog.size() > ROLLING_LOG_SIZE)
        m_rollingLog = m_rollingLog.substr(m_rollingLog.size() - ROLLING_LOG_SIZE);

    if (SRollingLogFollow::get().isRunning())
        SRollingLogFollow::get().addLog(str);

    if (!m_disableLogs || !**m_disableLogs) {
        // log to a file
        m_logOfs << str << "\n";
        m_logOfs.flush();
    }

    // log it to the stdout too.
    if (!m_disableStdout)
        std::println("{}", ((m_coloredLogs && !**m_coloredLogs) ? str : coloredStr));
}
