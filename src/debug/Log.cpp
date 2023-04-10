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

void Debug::log(LogLevel level, const char* fmt, ...) {

    if (disableLogs && *disableLogs)
        return;

    // log to a file
    std::ofstream ofs;
    ofs.open(logFile, std::ios::out | std::ios::app);

    switch (level) {
        case LOG: ofs << "[LOG] "; break;
        case WARN: ofs << "[WARN] "; break;
        case ERR: ofs << "[ERR] "; break;
        case CRIT: ofs << "[CRITICAL] "; break;
        case INFO: ofs << "[INFO] "; break;
        default: break;
    }

    // print date and time to the ofs
    if (disableTime && !*disableTime) {
        auto       timet  = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto MILLIS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() % 1000;

        ofs << std::put_time(std::localtime(&timet), "[%H:%M:%S:");

        if (MILLIS > 99)
            ofs << MILLIS;
        else if (MILLIS > 9)
            ofs << "0" << MILLIS;
        else
            ofs << "00" << MILLIS;

        ofs << "] ";
    }

    char*   outputStr = nullptr;

    va_list args;
    va_start(args, fmt);
    vasprintf(&outputStr, fmt, args);
    va_end(args);

    std::string output = std::string(outputStr);
    free(outputStr);

    ofs << output << "\n";

    ofs.close();

    // log it to the stdout too.
    if (!disableStdout)
        std::cout << output << "\n";
}
