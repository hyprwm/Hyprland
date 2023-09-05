#include "Log.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"

#include <fstream>
#include <iostream>

void Debug::init(const std::string& IS) {
    logFile = "/tmp/hypr/" + IS + (ISDEBUG ? "/hyprlandd.log" : "/hyprland.log");
}

void Debug::wlrLog(wlr_log_importance level, const char* fmt, va_list args) {
    char*         outputStr = nullptr;

    std::ofstream ofs;
    ofs.open(logFile, std::ios::out | std::ios::app);

    vasprintf(&outputStr, fmt, args);

    std::string output = std::string(outputStr);
    free(outputStr);

    ofs << "[wlr] " << output << "\n";

    ofs.close();

    if (!disableStdout)
        std::cout << output << "\n";
}
