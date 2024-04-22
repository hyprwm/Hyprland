#include "Log.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"

#include <fstream>
#include <iostream>

void Debug::init(const std::string& IS) {
    logFile = "/tmp/hypr/" + IS + (ISDEBUG ? "/hyprlandd.log" : "/hyprland.log");
}

void Debug::wlrLog(wlr_log_importance level, const char* fmt, va_list args) {
    if (level > wlr_log_get_verbosity())
        return;

    char* outputStr = nullptr;

    vasprintf(&outputStr, fmt, args);

    std::string output = std::string(outputStr);
    free(outputStr);

    rollingLog += output + "\n";

    if (!disableLogs || !**disableLogs) {
        std::ofstream ofs;
        ofs.open(logFile, std::ios::out | std::ios::app);
        ofs << "[wlr] " << output << "\n";
        ofs.close();
    }

    if (!disableStdout)
        std::cout << output << "\n";
}

void Debug::log(LogLevel level, std::string str) {
    if (level == TRACE && !trace)
        return;

    if (shuttingDown)
        return;

    switch (level) {
        case LOG: str = "[LOG] " + str; break;
        case WARN: str = "[WARN] " + str; break;
        case ERR: str = "[ERR] " + str; break;
        case CRIT: str = "[CRITICAL] " + str; break;
        case INFO: str = "[INFO] " + str; break;
        case TRACE: str = "[TRACE] " + str; break;
        default: break;
    }

    rollingLog += str + "\n";
    if (rollingLog.size() > ROLLING_LOG_SIZE)
        rollingLog = rollingLog.substr(rollingLog.size() - ROLLING_LOG_SIZE);

    if (!disableLogs || !**disableLogs) {
        // log to a file
        std::ofstream ofs;
        ofs.open(logFile, std::ios::out | std::ios::app);
        ofs << str << "\n";

        ofs.close();
    }

    // log it to the stdout too.
    if (!disableStdout)
        std::cout << str << "\n";
}