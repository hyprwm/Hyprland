#pragma once

#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <sys/types.h>

#include "../Log.hpp"

//NOLINTNEXTLINE
namespace Tests {
    Hyprutils::Memory::CUniquePointer<Hyprutils::OS::CProcess> spawnKitty(const std::string& class_ = "");
    bool                                                       processAlive(pid_t pid);
    int                                                        windowCount();
    int                                                        countOccurrences(const std::string& in, const std::string& what);
    bool                                                       killAllWindows();
};
