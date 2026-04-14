#include "SyncReleaser.hpp"
#include "SyncTimeline.hpp"
#include "../../render/OpenGL.hpp"
#include "../../helpers/Drm.hpp"

using namespace Hyprutils::OS;

CSyncReleaser::CSyncReleaser(SP<CSyncTimeline> timeline, uint64_t point) : m_timeline(timeline), m_point(point) {
    ;
}

CSyncReleaser::~CSyncReleaser() {
    if (!m_timeline) {
        Log::logger->log(Log::ERR, "CSyncReleaser destructing without a timeline");
        return;
    }

    if (m_fd.isValid())
        m_timeline->importFromSyncFileFD(m_point, m_fd);
    else
        m_timeline->signal(m_point);
}

void CSyncReleaser::addSyncFileFd(const Hyprutils::OS::CFileDescriptor& syncFd) {
    if (!m_fd.isValid())
        m_fd = syncFd.duplicate();
    else
        m_fd = DRM::mergeFence(m_fd.take(), syncFd.duplicate().take());
}

void CSyncReleaser::drop() {
    m_timeline.reset();
}
