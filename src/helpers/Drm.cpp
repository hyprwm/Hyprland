#include <xf86drm.h>

#ifdef __linux__
#include <linux/dma-buf.h>
#include <linux/sync_file.h>
#endif

#include <libsync.h>
#include <sys/ioctl.h>
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
#ifndef __linux__
    return {};
#else
    sync_accumulate("merged release fence", &fence1, fence2);

    if (fence2 >= 0)
        close(fence2);

    return CFileDescriptor{fence1};

#endif
}

void DRM::setDeadline(const Time::steady_tp& deadline, const Hyprutils::OS::CFileDescriptor& fence) {
#ifdef SYNC_IOC_SET_DEADLINE
    if (!fence.isValid())
        return;

    sync_set_deadline args{
        .deadline_ns = uint64_t(deadline.time_since_epoch().count()),
        .pad         = 0,
    };

    doIoctl(fence.get(), SYNC_IOC_SET_DEADLINE, &args);
#endif
}
