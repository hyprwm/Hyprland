#pragma once

#include <cstdint>
#include <optional>
#include <sys/types.h>
#include <hyprutils/os/FileDescriptor.hpp>

namespace DRM {
    std::optional<dev_t>           devIDFromFD(int fd);
    bool                           sameGpu(int fd1, int fd2);
    int                            doIoctl(int fd, unsigned long request, void* arg);
    Hyprutils::OS::CFileDescriptor exportFence(int fd);
    Hyprutils::OS::CFileDescriptor mergeFence(const Hyprutils::OS::CFileDescriptor& fd1, const Hyprutils::OS::CFileDescriptor& fd2);
    bool                           setDeadline(uint64_t deadlineNs, const Hyprutils::OS::CFileDescriptor& fence);
}
