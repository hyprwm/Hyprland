#include "Log.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"

#include <fstream>
#include <iostream>

void Debug::init(std::string IS) {
    if (ISDEBUG)
        logFile = "/tmp/hypr/" + IS + "/hyprlandd.log";
    else
        logFile = "/tmp/hypr/" + IS + "/hyprland.log";
}

void Debug::log(LogLevel level, const char* fmt, ...) {

    if (disableLogs && *disableLogs)
        return;

    // log to a file
    std::ofstream ofs;
    ofs.open(logFile, std::ios::out | std::ios::app);

    switch (level) {
        case LOG:
            ofs << "[LOG] ";
            break;
        case WARN:
            ofs << "[WARN] ";
            break;
        case ERR:
            ofs << "[ERR] ";
            break;
        case CRIT:
            ofs << "[CRITICAL] ";
            break;
        case INFO:
            ofs << "[INFO] ";
            break;
        default:
            break;
    }

    // print date and time to the ofs
    if (disableTime && !*disableTime) {
        auto timet = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
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

    char buf[LOGMESSAGESIZE] = "";
    char* outputStr;
    int logLen;

    va_list args;
    va_start(args, fmt);
    logLen = vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);

    if ((long unsigned int)logLen < sizeof buf) {
        outputStr = strdup(buf);
    } else {
        outputStr = (char*)malloc(logLen + 1);

        if (!outputStr) {
            printf("CRITICAL: Cannot alloc size %d for log! (Out of memory?)", logLen + 1);
            return;
        }

        va_start(args, fmt);
        vsnprintf(outputStr, logLen + 1U, fmt, args);
        va_end(args);
    }

    ofs << outputStr << "\n";

    ofs.close();

    // log it to the stdout too.
    std::cout << outputStr << "\n";

    // free the log
    free(outputStr);
}
