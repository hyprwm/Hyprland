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
