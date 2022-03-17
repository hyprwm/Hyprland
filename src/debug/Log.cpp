#include "Log.hpp"
#include "../defines.hpp"

#include <fstream>
#include <iostream>

void Debug::log(LogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // log to a file
    const std::string DEBUGPATH = ISDEBUG ? "/tmp/hypr/hyprlandd.log" : "/tmp/hypr/hyprland.log";
    std::ofstream ofs;
    ofs.open(DEBUGPATH, std::ios::out | std::ios::app);

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

    vsprintf(buf, fmt, args);

    ofs << buf << "\n";

    ofs.close();

    // log it to the stdout too.
    std::cout << buf << "\n";

    va_end(args);
}
