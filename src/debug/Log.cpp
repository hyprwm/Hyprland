#include "Log.hpp"
#include "../defines.hpp"

#include <fstream>
#include <iostream>

void Debug::init() {
    if (ISDEBUG)
        logFile = "/tmp/hypr/hyprlandd-" + std::to_string(time(NULL)) + ".log";
    else
        logFile = "/tmp/hypr/hyprland-" + std::to_string(time(NULL)) + ".log";
}

void Debug::log(LogLevel level, const char* fmt, ...) {

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
        default:
            break;
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
