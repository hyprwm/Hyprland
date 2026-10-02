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
        LOG(Log::ERR, "CSyncReleaser destructing without a timeline");
        return;
    }

    if (m_fd.isValid())
        m_timeline->importFromSyncFileFD(m_point, m_fd);
    else
        m_timeline->signal(m_point);
}

bool CSyncReleaser::addSyncFileFd(const Hyprutils::OS::CFileDescriptor& syncFd) {
    auto fd = m_fd.isValid() ? DRM::mergeFence(m_fd, syncFd) : syncFd.duplicate();
    if (!fd.isValid())
        return false;

    m_fd = std::move(fd);
    return true;
}

void CSyncReleaser::drop() {
    m_timeline.reset();
}
