#include "SyncTimeline.hpp"
#include "../../defines.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"

#include <xf86drm.h>
#include <sys/eventfd.h>
using namespace Hyprutils::OS;

SP<CSyncTimeline> CSyncTimeline::create(int drmFD_) {
    auto timeline   = SP<CSyncTimeline>(new CSyncTimeline);
    timeline->drmFD = drmFD_;
    timeline->self  = timeline;

    if (drmSyncobjCreate(drmFD_, 0, &timeline->handle)) {
        Debug::log(ERR, "CSyncTimeline: failed to create a drm syncobj??");
        return nullptr;
    }

    return timeline;
}

SP<CSyncTimeline> CSyncTimeline::create(int drmFD_, CFileDescriptor&& drmSyncobjFD) {
    auto timeline       = SP<CSyncTimeline>(new CSyncTimeline);
    timeline->drmFD     = drmFD_;
    timeline->syncobjFd = std::move(drmSyncobjFD);
    timeline->self      = timeline;

    if (drmSyncobjFDToHandle(drmFD_, timeline->syncobjFd.get(), &timeline->handle)) {
        Debug::log(ERR, "CSyncTimeline: failed to create a drm syncobj from fd??");
        return nullptr;
    }

    return timeline;
}

CSyncTimeline::~CSyncTimeline() {
    if (handle == 0)
        return;

    drmSyncobjDestroy(drmFD, handle);
}

std::optional<bool> CSyncTimeline::check(uint64_t point, uint32_t flags) {
#ifdef __FreeBSD__
    constexpr int ETIME_ERR = ETIMEDOUT;
#else
    constexpr int ETIME_ERR = ETIME;
#endif

    uint32_t signaled = 0;
    int      ret      = drmSyncobjTimelineWait(drmFD, &handle, &point, 1, 0, flags, &signaled);
    if (ret != 0 && ret != -ETIME_ERR) {
        Debug::log(ERR, "CSyncTimeline::check: drmSyncobjTimelineWait failed");
        return std::nullopt;
    }

    return ret == 0;
}

bool CSyncTimeline::addWaiter(const std::function<void()>& waiter, uint64_t point, uint32_t flags) {
    auto eventFd = CFileDescriptor(eventfd(0, EFD_CLOEXEC));

    if (!eventFd.isValid()) {
        Debug::log(ERR, "CSyncTimeline::addWaiter: failed to acquire an eventfd");
        return false;
    }

    if (drmSyncobjEventfd(drmFD, handle, point, eventFd.get(), flags)) {
        Debug::log(ERR, "CSyncTimeline::addWaiter: drmSyncobjEventfd failed");
        return false;
    }

    g_pEventLoopManager->doOnReadable(std::move(eventFd), waiter);

    return true;
}

CFileDescriptor CSyncTimeline::exportAsSyncFileFD(uint64_t src) {
    int      sync = -1;

    uint32_t syncHandle = 0;
    if (drmSyncobjCreate(drmFD, 0, &syncHandle)) {
        Debug::log(ERR, "exportAsSyncFileFD: drmSyncobjCreate failed");
        return {};
    }

    if (drmSyncobjTransfer(drmFD, syncHandle, 0, handle, src, 0)) {
        Debug::log(ERR, "exportAsSyncFileFD: drmSyncobjTransfer failed");
        drmSyncobjDestroy(drmFD, syncHandle);
        return {};
    }

    if (drmSyncobjExportSyncFile(drmFD, syncHandle, &sync)) {
        Debug::log(ERR, "exportAsSyncFileFD: drmSyncobjExportSyncFile failed");
        drmSyncobjDestroy(drmFD, syncHandle);
        return {};
    }

    drmSyncobjDestroy(drmFD, syncHandle);
    return CFileDescriptor{sync};
}

bool CSyncTimeline::importFromSyncFileFD(uint64_t dst, CFileDescriptor& fd) {
    uint32_t syncHandle = 0;

    if (drmSyncobjCreate(drmFD, 0, &syncHandle)) {
        Debug::log(ERR, "importFromSyncFileFD: drmSyncobjCreate failed");
        return false;
    }

    if (drmSyncobjImportSyncFile(drmFD, syncHandle, fd.get())) {
        Debug::log(ERR, "importFromSyncFileFD: drmSyncobjImportSyncFile failed");
        drmSyncobjDestroy(drmFD, syncHandle);
        return false;
    }

    if (drmSyncobjTransfer(drmFD, handle, dst, syncHandle, 0, 0)) {
        Debug::log(ERR, "importFromSyncFileFD: drmSyncobjTransfer failed");
        drmSyncobjDestroy(drmFD, syncHandle);
        return false;
    }

    drmSyncobjDestroy(drmFD, syncHandle);
    return true;
}

bool CSyncTimeline::transfer(SP<CSyncTimeline> from, uint64_t fromPoint, uint64_t toPoint) {
    if (drmFD != from->drmFD) {
        Debug::log(ERR, "CSyncTimeline::transfer: cannot transfer timelines between gpus, {} -> {}", from->drmFD, drmFD);
        return false;
    }

    if (drmSyncobjTransfer(drmFD, handle, toPoint, from->handle, fromPoint, 0)) {
        Debug::log(ERR, "CSyncTimeline::transfer: drmSyncobjTransfer failed");
        return false;
    }

    return true;
}

void CSyncTimeline::signal(uint64_t point) {
    if (drmSyncobjTimelineSignal(drmFD, &handle, &point, 1))
        Debug::log(ERR, "CSyncTimeline::signal: drmSyncobjTimelineSignal failed");
}
