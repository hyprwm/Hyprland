#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <functional>
#include <hyprutils/os/FileDescriptor.hpp>
#include "../memory/Memory.hpp"

/*
    A helper (inspired by KDE's KWin) that will release the timeline point in the dtor
*/

class CSyncTimeline;
class CEGLSync;

class CSyncReleaser {
  public:
    CSyncReleaser(SP<CSyncTimeline> timeline_, uint64_t point_);
    ~CSyncReleaser();

    // drops the releaser, will never signal anymore
    void drop();

    // wait for this gpu job to finish before releasing
    Hyprutils::OS::CFileDescriptor mergeSyncFds(const Hyprutils::OS::CFileDescriptor& fd1, const Hyprutils::OS::CFileDescriptor& fd2);
    void                           addReleaseSync(SP<CEGLSync> sync);

  private:
    SP<CSyncTimeline>              timeline;
    uint64_t                       point = 0;
    Hyprutils::OS::CFileDescriptor m_fd;
    SP<CEGLSync>                   m_sync;
};
