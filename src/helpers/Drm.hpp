#pragma once

#include <optional>
#include <sys/types.h>
#include <hyprutils/os/FileDescriptor.hpp>
#include "time/Time.hpp"

namespace DRM {
    std::optional<dev_t>           devIDFromFD(int fd);
    bool                           sameGpu(int fd1, int fd2);
    int                            doIoctl(int fd, unsigned long request, void* arg);
    std::optional<Time::steady_tp> fenceSignalTime(int fd);
    Hyprutils::OS::CFileDescriptor exportFence(int fd);
    Hyprutils::OS::CFileDescriptor mergeFence(const Hyprutils::OS::CFileDescriptor& fd1, const Hyprutils::OS::CFileDescriptor& fd2);
}
