#include <xf86drm.h>
#include <array>
#include <map>
#include <mutex>
#include <optional>
#include <sys/stat.h>
#include <hyprutils/os/FileDescriptor.hpp>
#include <string>
#include <algorithm>
#include "Drm.hpp"
#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/dma-buf.h>
#include <linux/sync_file.h>
#else
struct sync_merge_data {
    char  name[32];
    __s32 fd2;
    __s32 fence;
    __u32 flags;
    __u32 pad;
};
#define SYNC_IOC_MAGIC '>'
#define SYNC_IOC_MERGE _IOWR(SYNC_IOC_MAGIC, 3, struct sync_merge_data)
#endif

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
    if (fd < 0)
        return {};

    CFileDescriptor fence;
#ifdef __linux__
    dma_buf_export_sync_file request{
        .flags = DMA_BUF_SYNC_READ,
        .fd    = -1,
    };

    if (doIoctl(fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &request) == 0)
        fence = CFileDescriptor{request.fd};
#endif

    return fence;
}

CFileDescriptor DRM::mergeFence(const CFileDescriptor& fd1, const CFileDescriptor& fd2) {
    if (!fd1.isValid() || !fd2.isValid())
        return {};

    CFileDescriptor mergedFence;
#ifdef __linux__
    struct sync_merge_data data{
        .name  = "merged release fence",
        .fd2   = fd2.get(),
        .fence = -1,
    };

    if (doIoctl(fd1.get(), SYNC_IOC_MERGE, &data) == 0)
        mergedFence = CFileDescriptor(data.fence);
    else
        mergedFence = {};

#endif
    return mergedFence;
}
