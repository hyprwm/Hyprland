#pragma once

#include <optional>
#include <sys/types.h>
#include <hyprutils/os/FileDescriptor.hpp>
#include "time/Time.hpp"

namespace DRM {
    std::optional<dev_t>           devIDFromFD(int fd);
    bool                           sameGpu(int fd1, int fd2);
    int                            doIoctl(int fd, unsigned long request, void* arg);
    Hyprutils::OS::CFileDescriptor exportFence(int fd);
    Hyprutils::OS::CFileDescriptor mergeFence(int fence1, int fence2);
    void                           setDeadline(const Time::steady_tp& deadline, const Hyprutils::OS::CFileDescriptor& fence);
}
