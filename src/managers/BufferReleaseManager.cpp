#include "BufferReleaseManager.hpp"
#include "../helpers/Monitor.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include "render/OpenGL.hpp"

#include <sys/ioctl.h>

using namespace Hyprutils::OS;

#if defined(__linux__)
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

static CFileDescriptor mergeSyncFds(const CFileDescriptor& fd1, const CFileDescriptor& fd2) {
    // combines the fences of both sync_fds into a dma_fence_array (https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html#c.dma_fence_array_create)
    // with the signal_on_any param set to false, so the new sync_fd will "signal when all fences in the array signal."

    struct sync_merge_data data{
        .name  = "merged release fence",
        .fd2   = fd2.get(),
        .fence = -1,
    };
    int err = -1;
    do {
        err = ioctl(fd1.get(), SYNC_IOC_MERGE, &data);
    } while (err == -1 && (errno == EINTR || errno == EAGAIN));
    if (err < 0)
        return CFileDescriptor{};
    else
        return CFileDescriptor(data.fence);
}

bool CBufferReleaseManager::addBuffer(PHLMONITORREF monitor, const CHLBufferReference& buf) {
    if (!monitor)
        return false;

    auto it = std::ranges::find_if(m_buffers[monitor], [&buf](auto& b) { return b == buf; });

    if (it != m_buffers[monitor].end())
        return false;

    m_buffers[monitor].emplace_back(buf);
    return true;
}

void CBufferReleaseManager::addFence(PHLMONITORREF monitor) {
    if (g_pHyprOpenGL->explicitSyncSupported() && monitor->m_inFence.isValid()) {
        for (auto& b : m_buffers[monitor]) {
            if (!b->m_syncReleasers.empty()) {
                for (auto& releaser : b->m_syncReleasers) {
                    releaser->addSyncFileFd(monitor->m_inFence);
                }
            } else {
                if (b->m_syncDropFd.isValid())
                    b->m_syncDropFd = mergeSyncFds(b->m_syncDropFd, monitor->m_inFence.duplicate());
                else
                    b->m_syncDropFd = monitor->m_inFence.duplicate();
            }
        }
    }
}

void CBufferReleaseManager::dropBuffers(PHLMONITORREF monitor) {
    if (g_pHyprOpenGL->explicitSyncSupported()) {
        std::erase_if(m_buffers[monitor], [](auto& b) { return !b->m_syncReleasers.empty(); });
        for (auto& b : m_buffers[monitor]) {
            g_pEventLoopManager->doOnReadable(b->m_syncDropFd.duplicate(), [buf = b] mutable {});
        }
    }

    m_buffers[monitor].clear();
    std::erase_if(m_buffers, [](auto& b) { return !b.first; }); // if monitor is gone.
}

void CBufferReleaseManager::destroy(PHLMONITORREF monitor) {
    m_buffers.erase(monitor);
}
