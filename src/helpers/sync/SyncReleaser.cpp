#include "SyncReleaser.hpp"
#include "SyncTimeline.hpp"
#include "../../render/OpenGL.hpp"

CSyncReleaser::CSyncReleaser(SP<CSyncTimeline> timeline_, uint64_t point_) : timeline(timeline_), point(point_) {
    ;
}

CSyncReleaser::~CSyncReleaser() {
    if (!timeline) {
        Debug::log(ERR, "CSyncReleaser destructing without a timeline");
        return;
    }

    if (sync)
        timeline->importFromSyncFileFD(point, sync->fd());
    else
        timeline->signal(point);
}

void CSyncReleaser::addReleaseSync(SP<CEGLSync> sync_) {
    sync = sync_;
}

void CSyncReleaser::drop() {
    timeline.reset();
}
