#include <xf86drm.h>
#include <libsync.h>

#if defined(__linux__)
#include <linux/dma-buf.h>
#endif

#include "Drm.hpp"
using namespace Hyprutils::OS;

bool DRM::sameGpu(int fd1, int fd2) {
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
