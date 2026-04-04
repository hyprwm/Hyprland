#pragma once

#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <sys/types.h>

#include "../Log.hpp"

//NOLINTNEXTLINE
namespace Tests {
    Hyprutils::Memory::CUniquePointer<Hyprutils::OS::CProcess> spawnKitty(const std::string& class_ = "", const std::vector<std::string> args = {});
    Hyprutils::Memory::CUniquePointer<Hyprutils::OS::CProcess> spawnLayerKitty(const std::string& namespace_ = "", const std::vector<std::string> args = {});
    bool                                                       processAlive(pid_t pid);
    int                                                        windowCount();
    int                                                        countOccurrences(const std::string& in, const std::string& what);
    bool                                                       killAllWindows();
    void                                                       waitUntilWindowsN(int n);
    int                                                        layerCount();
    bool                                                       killAllLayers();
    std::string                                                execAndGet(const std::string& cmd);
    bool                                                       writeFile(const std::string& name, const std::string& contents);
    std::string                                                getWindowAttribute(const std::string& winInfo, const std::string& attr);
};
