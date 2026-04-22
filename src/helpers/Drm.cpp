#include <xf86drm.h>
#include <array>
#include <map>
#include <mutex>
#include <optional>
#include <sys/stat.h>

#if defined(__linux__)
#include <linux/dma-buf.h>
#include <linux/sync_file.h>
#endif

#include <libsync.h>
#include <sys/ioctl.h>

#include "Drm.hpp"

using namespace Hyprutils::OS;

namespace {
    using SDRMNodePair = std::array<dev_t, 2>;

    std::optional<SDRMNodePair> getDrmNodePair(int fd1, int fd2) {
        const auto DEVA = DRM::devIDFromFD(fd1);
        const auto DEVB = DRM::devIDFromFD(fd2);
        if (!DEVA || !DEVB)
            return std::nullopt;

        SDRMNodePair pair = {*DEVA, *DEVB};
        if (pair[0] > pair[1])
            std::swap(pair[0], pair[1]);

        return pair;
    }
}

std::optional<dev_t> DRM::devIDFromFD(int fd) {
    struct stat stat = {};
    if (fstat(fd, &stat) != 0 || !S_ISCHR(stat.st_mode))
        return std::nullopt;

    return stat.st_rdev;
}

bool DRM::sameGpu(int fd1, int fd2) {
    if (fd1 < 0 || fd2 < 0 || fd1 == fd2)
        return true;

    static std::mutex                   cacheMutex;
    static std::map<SDRMNodePair, bool> sameGpuCache;

    const auto                          NODEPAIR = getDrmNodePair(fd1, fd2);
    if (NODEPAIR) {
        std::scoped_lock lock(cacheMutex);
        if (const auto it = sameGpuCache.find(*NODEPAIR); it != sameGpuCache.end())
            return it->second;
    }

    drmDevice* devA = nullptr;
    drmDevice* devB = nullptr;

    if (drmGetDevice2(fd1, 0, &devA) != 0)
        return false;
    if (drmGetDevice2(fd2, 0, &devB) != 0) {
        drmFreeDevice(&devA);
        return false;
    }

    bool same = drmDevicesEqual(devA, devB);

    drmFreeDevice(&devA);
    drmFreeDevice(&devB);

    if (NODEPAIR) {
        std::scoped_lock lock(cacheMutex);
        sameGpuCache[*NODEPAIR] = same;
    }

    return same;
}

int DRM::doIoctl(int fd, unsigned long request, void* arg) {
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    return ret;
}

// https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html#c.dma_buf_export_sync_file
// returns a sync file that will be signalled when dmabuf is ready to be read
CFileDescriptor DRM::exportFence(int fd) {
#ifndef __linux__
    return {};
#endif

#ifdef DMA_BUF_IOCTL_EXPORT_SYNC_FILE
    if (fd < 0)
        return {};

    dma_buf_export_sync_file request{
        .flags = DMA_BUF_SYNC_READ,
        .fd    = -1,
    };

    if (doIoctl(fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &request) == 0)
        return CFileDescriptor{request.fd};
#endif
    return {};
}

CFileDescriptor DRM::mergeFence(int fence1, int fence2) {
#ifdef __linux__
    sync_accumulate("merged release fence", &fence1, fence2);

    if (fence2 >= 0)
        close(fence2);

    return CFileDescriptor{fence1};
#else
    return {};
#endif
}

void DRM::setDeadline(const Time::steady_tp& deadline, const Hyprutils::OS::CFileDescriptor& fence) {
#ifdef SYNC_IOC_SET_DEADLINE
    if (!fence.isValid())
        return;

    sync_set_deadline args{
        .deadline_ns = uint64_t(duration_cast<std::chrono::nanoseconds>(deadline.time_since_epoch()).count()),
        .pad         = 0,
    };

    doIoctl(fence.get(), SYNC_IOC_SET_DEADLINE, &args);
#endif
}