#pragma once

#include <optional>
#include <sys/types.h>

namespace DRM {
    std::optional<dev_t> devIDFromFD(int fd);
    bool                 sameGpu(int fd1, int fd2);
}
