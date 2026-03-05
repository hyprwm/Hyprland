#pragma once
#include "time/Time.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

namespace DRM {
    bool                           sameGpu(int fd1, int fd2);
    int                            doIoctl(int fd, unsigned long request, void* arg);
    Hyprutils::OS::CFileDescriptor exportFence(int fd);
    Hyprutils::OS::CFileDescriptor mergeFence(int fence1, int fence2);
    void                           setDeadline(const Time::steady_tp& deadline, const Hyprutils::OS::CFileDescriptor& fence);
}
