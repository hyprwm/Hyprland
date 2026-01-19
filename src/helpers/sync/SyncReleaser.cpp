#include "SyncReleaser.hpp"
#include "SyncTimeline.hpp"
#include "../../render/OpenGL.hpp"
#include <sys/ioctl.h>

using namespace Hyprutils::OS;

CSyncReleaser::CSyncReleaser(SP<CSyncTimeline> timeline, uint64_t point) : m_timeline(timeline), m_point(point) {
    ;
}

CSyncReleaser::~CSyncReleaser() {
    if (!m_timeline) {
        Log::logger->log(Log::ERR, "CSyncReleaser destructing without a timeline");
        return;
    }

    if (m_fence.isValid())
        m_timeline->importFromSyncFileFD(m_point, m_fence.fd());
    else
        m_timeline->signal(m_point);
}

void CSyncReleaser::addSyncFileFd(const Hyprutils::OS::CFileDescriptor& syncFd) {
    m_fence.merge(syncFd.duplicate());
}

void CSyncReleaser::drop() {
    m_timeline.reset();
}
