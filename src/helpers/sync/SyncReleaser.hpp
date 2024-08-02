#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <functional>
#include "../memory/Memory.hpp"

/*
    A helper (inspired by KDE's KWin) that will release the timeline point in the dtor
*/

class CSyncTimeline;

class CSyncReleaser {
  public:
    CSyncReleaser(WP<CSyncTimeline> timeline_, uint64_t point_);
    ~CSyncReleaser();

    // drops the releaser, will never signal anymore
    void drop();

    // wait for this gpu job to finish before releasing
    void addReleaseSyncFD(int syncFD);

  private:
    WP<CSyncTimeline> timeline;
    uint64_t          point = 0;
    int               fd    = -1;
};
