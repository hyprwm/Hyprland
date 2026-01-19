#include <sys/ioctl.h>
#include <xf86drm.h>
#include <hyprutils/os/FileDescriptor.hpp>

#include "Fence.hpp"
#include "../debug/log/Logger.hpp"

#if defined(__linux__)
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

// https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html#c.dma_buf_export_sync_file
// returns a sync file that will be signalled when dmabuf is ready to be read

using namespace Hyprutils::OS;

CFence::CFence(int fd) {
    if (fd == -1)
        return;

#ifdef DMA_BUF_IOCTL_EXPORT_SYNC_FILE
    dma_buf_export_sync_file req{
        .flags = DMA_BUF_SYNC_READ,
        .fd    = -1,
    };
    if (drmIoctl(fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &req) == 0) {
        m_fence = CFileDescriptor{req.fd};
    }
#endif
}

CFence::CFence(std::array<int, 4> fds) {
    if (std::all_of(fds.begin(), fds.end(), [](int fd) { return fd == -1; }))
        return;

#ifndef __linux__
    return;
#endif

    std::vector<CFileDescriptor> syncFds;
    syncFds.reserve(fds.size());

    for (const auto& fd : fds) {
        if (fd == -1)
            continue;

        dma_buf_export_sync_file request{
            .flags = DMA_BUF_SYNC_READ,
            .fd    = -1,
        };

        if (doIoctl(fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &request) == 0)
            syncFds.emplace_back(request.fd);
    }

    if (syncFds.empty())
        return;

    CFileDescriptor syncFd;
    for (auto& fd : syncFds) {
        if (!syncFd.isValid()) {
            syncFd = std::move(fd);
            continue;
        }

        const std::string      name = "merged release fence";
        struct sync_merge_data data{
            .name  = {}, // zero-initialize name[]
            .fd2   = fd.get(),
            .fence = -1,
        };

        std::ranges::copy_n(name.c_str(), std::min(name.size() + 1, sizeof(data.name)), data.name);

        if (doIoctl(syncFd.get(), SYNC_IOC_MERGE, &data) == 0)
            syncFd = CFileDescriptor(data.fence);
        else
            syncFd = {};
    }

    m_fence = std::move(syncFd);
}

int CFence::doIoctl(int fd, unsigned long request, void* arg) {
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    return ret;
}

bool CFence::isValid() {
    return m_fence.isValid();
}

const CFileDescriptor& CFence::fd() {
    return m_fence;
}

void CFence::setDeadline(const Time::steady_tp& deadline) {
#ifdef SYNC_IOC_SET_DEADLINE
    if (!m_fence.isValid()) {
        return;
    }

    sync_set_deadline args{
        .deadline_ns = uint64_t(deadline.time_since_epoch().count()),
        .pad         = 0,
    };
    drmIoctl(m_fence.get(), SYNC_IOC_SET_DEADLINE, &args);
#endif
}

void CFence::merge(CFileDescriptor&& fence) {
    if (m_fence.isValid()) {
        struct sync_merge_data data{
            .name  = "merged release fence",
            .fd2   = fence.get(),
            .fence = -1,
        };

        int err = -1;
        do {
            err = ioctl(m_fence.get(), SYNC_IOC_MERGE, &data);
        } while (err == -1 && (errno == EINTR || errno == EAGAIN));
        if (err < 0) {
            Log::logger->log(Log::ERR, "CFence::mergeWithFence failed to merge fences");
            return;
        }

        m_fence = CFileDescriptor{data.fence};
    } else
        m_fence = CFileDescriptor{std::move(fence)};
}
