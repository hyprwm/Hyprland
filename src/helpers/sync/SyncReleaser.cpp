#include "SyncReleaser.hpp"
#include "SyncTimeline.hpp"
#include "../../render/OpenGL.hpp"

CSyncReleaser::CSyncReleaser(WP<CSyncTimeline> timeline_, uint64_t point_) : timeline(timeline_), point(point_) {
    ;
}

CSyncReleaser::~CSyncReleaser() {
    if (timeline.expired())
        return;

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