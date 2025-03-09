#include "SyncReleaser.hpp"
#include "SyncTimeline.hpp"
#include "../../render/OpenGL.hpp"
#include <sys/ioctl.h>

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

using namespace Hyprutils::OS;

CSyncReleaser::CSyncReleaser(SP<CSyncTimeline> timeline_, uint64_t point_) : timeline(timeline_), point(point_) {
    ;
}

CSyncReleaser::~CSyncReleaser() {
    if (!timeline) {
        Debug::log(ERR, "CSyncReleaser destructing without a timeline");
        return;
    }

    if (m_fd.isValid())
        timeline->importFromSyncFileFD(point, m_fd);
    else
        timeline->signal(point);
}

CFileDescriptor CSyncReleaser::mergeSyncFds(const CFileDescriptor& fd1, const CFileDescriptor& fd2) {
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

void CSyncReleaser::addReleaseSync(SP<CEGLSync> sync) {
    if (m_fd.isValid())
        m_fd = mergeSyncFds(m_fd, sync->takeFD());
    else
        m_fd = sync->fd().duplicate();

    m_sync = sync;
}

void CSyncReleaser::drop() {
    timeline.reset();
}
