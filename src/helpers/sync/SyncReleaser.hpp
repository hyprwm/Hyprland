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
    CSyncReleaser(SP<CSyncTimeline> timeline, uint64_t point);
    ~CSyncReleaser();

    // drops the releaser, will never signal anymore
    void drop();

    // wait for this sync_fd to signal before releasing
    void addSyncFileFd(const Hyprutils::OS::CFileDescriptor& syncFd);

  private:
    SP<CSyncTimeline>              m_timeline;
    uint64_t                       m_point = 0;
    Hyprutils::OS::CFileDescriptor m_fd;
    SP<CEGLSync>                   m_sync;
};
