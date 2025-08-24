#include "SyncTimeline.hpp"
#include "../../defines.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../Compositor.hpp"

#include <xf86drm.h>
#include <sys/eventfd.h>
using namespace Hyprutils::OS;

SP<CSyncTimeline> CSyncTimeline::create(int drmFD_) {
    if (!g_pCompositor->supportsDrmSyncobjTimeline())
        return nullptr;

    auto timeline     = SP<CSyncTimeline>(new CSyncTimeline);
    timeline->m_drmFD = drmFD_;
    timeline->m_self  = timeline;

    if (drmSyncobjCreate(drmFD_, 0, &timeline->m_handle)) {
        Debug::log(ERR, "CSyncTimeline: failed to create a drm syncobj??");
        return nullptr;
    }

    return timeline;
}

SP<CSyncTimeline> CSyncTimeline::create(int drmFD_, CFileDescriptor&& drmSyncobjFD) {
    if (!g_pCompositor->supportsDrmSyncobjTimeline())
        return nullptr;

    auto timeline         = SP<CSyncTimeline>(new CSyncTimeline);
    timeline->m_drmFD     = drmFD_;
    timeline->m_syncobjFD = std::move(drmSyncobjFD);
    timeline->m_self      = timeline;

    if (drmSyncobjFDToHandle(drmFD_, timeline->m_syncobjFD.get(), &timeline->m_handle)) {
        Debug::log(ERR, "CSyncTimeline: failed to create a drm syncobj from fd??");
        return nullptr;
    }

    return timeline;
}

CSyncTimeline::~CSyncTimeline() {
    if (m_handle == 0)
        return;

    drmSyncobjDestroy(m_drmFD, m_handle);
}

std::optional<bool> CSyncTimeline::check(uint64_t point, uint32_t flags) {
#ifdef __FreeBSD__
    constexpr int ETIME_ERR = ETIMEDOUT;
#else
    constexpr int ETIME_ERR = ETIME;
#endif

    uint32_t signaled = 0;
    int      ret      = drmSyncobjTimelineWait(m_drmFD, &m_handle, &point, 1, 0, flags, &signaled);
    if (ret != 0 && ret != -ETIME_ERR) {
        Debug::log(ERR, "CSyncTimeline::check: drmSyncobjTimelineWait failed");
        return std::nullopt;
    }

    return ret == 0;
}

bool CSyncTimeline::addWaiter(std::function<void()>&& waiter, uint64_t point, uint32_t flags) {
    auto eventFd = CFileDescriptor(eventfd(0, EFD_CLOEXEC));

    if (!eventFd.isValid()) {
        Debug::log(ERR, "CSyncTimeline::addWaiter: failed to acquire an eventfd");
        return false;
    }

    if (drmSyncobjEventfd(m_drmFD, m_handle, point, eventFd.get(), flags)) {
        Debug::log(ERR, "CSyncTimeline::addWaiter: drmSyncobjEventfd failed");
        return false;
    }

    g_pEventLoopManager->doOnReadable(std::move(eventFd), std::move(waiter));

    return true;
}

CFileDescriptor CSyncTimeline::exportAsSyncFileFD(uint64_t src) {
    int      sync = -1;

    uint32_t syncHandle = 0;
    if (drmSyncobjCreate(m_drmFD, 0, &syncHandle)) {
        Debug::log(ERR, "exportAsSyncFileFD: drmSyncobjCreate failed");
        return {};
    }

    if (drmSyncobjTransfer(m_drmFD, syncHandle, 0, m_handle, src, 0)) {
        Debug::log(ERR, "exportAsSyncFileFD: drmSyncobjTransfer failed");
        drmSyncobjDestroy(m_drmFD, syncHandle);
        return {};
    }

    if (drmSyncobjExportSyncFile(m_drmFD, syncHandle, &sync)) {
        Debug::log(ERR, "exportAsSyncFileFD: drmSyncobjExportSyncFile failed");
        drmSyncobjDestroy(m_drmFD, syncHandle);
        return {};
    }

    drmSyncobjDestroy(m_drmFD, syncHandle);
    return CFileDescriptor{sync};
}

bool CSyncTimeline::importFromSyncFileFD(uint64_t dst, CFileDescriptor& fd) {
    uint32_t syncHandle = 0;

    if (drmSyncobjCreate(m_drmFD, 0, &syncHandle)) {
        Debug::log(ERR, "importFromSyncFileFD: drmSyncobjCreate failed");
        return false;
    }

    if (drmSyncobjImportSyncFile(m_drmFD, syncHandle, fd.get())) {
        Debug::log(ERR, "importFromSyncFileFD: drmSyncobjImportSyncFile failed");
        drmSyncobjDestroy(m_drmFD, syncHandle);
        return false;
    }

    if (drmSyncobjTransfer(m_drmFD, m_handle, dst, syncHandle, 0, 0)) {
        Debug::log(ERR, "importFromSyncFileFD: drmSyncobjTransfer failed");
        drmSyncobjDestroy(m_drmFD, syncHandle);
        return false;
    }

    drmSyncobjDestroy(m_drmFD, syncHandle);
    return true;
}

bool CSyncTimeline::transfer(SP<CSyncTimeline> from, uint64_t fromPoint, uint64_t toPoint) {
    if (m_drmFD != from->m_drmFD) {
        Debug::log(ERR, "CSyncTimeline::transfer: cannot transfer timelines between gpus, {} -> {}", from->m_drmFD, m_drmFD);
        return false;
    }

    if (drmSyncobjTransfer(m_drmFD, m_handle, toPoint, from->m_handle, fromPoint, 0)) {
        Debug::log(ERR, "CSyncTimeline::transfer: drmSyncobjTransfer failed");
        return false;
    }

    return true;
}

void CSyncTimeline::signal(uint64_t point) {
    if (drmSyncobjTimelineSignal(m_drmFD, &m_handle, &point, 1))
        Debug::log(ERR, "CSyncTimeline::signal: drmSyncobjTimelineSignal failed");
}
