#pragma once

#include <hyprutils/os/Process.hpp>
#include <sys/types.h>

//NOLINTNEXTLINE
namespace Tests {
    Hyprutils::OS::CProcess spawnKitty();
    bool                    processAlive(pid_t pid);
    int                     windowCount();
    int                     countOccurrences(const std::string& in, const std::string& what);
};
