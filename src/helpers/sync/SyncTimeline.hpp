#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <functional>
#include <hyprutils/os/FileDescriptor.hpp>
#include "../memory/Memory.hpp"

/*
    Hyprland synchronization timelines are based on the wlroots' ones, which
    are based on Vk timeline semaphores: https://www.khronos.org/blog/vulkan-timeline-semaphores
*/

struct wl_event_source;

class CSyncTimeline {
  public:
    static SP<CSyncTimeline> create(int drmFD_);
    static SP<CSyncTimeline> create(int drmFD_, Hyprutils::OS::CFileDescriptor&& drmSyncobjFD);
    ~CSyncTimeline();

    struct SWaiter {
        std::function<void()>          fn;
        wl_event_source*               source = nullptr;
        WP<CSyncTimeline>              timeline;
        Hyprutils::OS::CFileDescriptor eventFd;
    };

    // check if the timeline point has been signaled
    // flags: DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT or DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE
    // std::nullopt on fail
    std::optional<bool>            check(uint64_t point, uint32_t flags);

    bool                           addWaiter(const std::function<void()>& waiter, uint64_t point, uint32_t flags);
    void                           removeWaiter(SWaiter*);
    void                           removeAllWaiters();
    Hyprutils::OS::CFileDescriptor exportAsSyncFileFD(uint64_t src);
    bool                           importFromSyncFileFD(uint64_t dst, Hyprutils::OS::CFileDescriptor& fd);
    bool                           transfer(SP<CSyncTimeline> from, uint64_t fromPoint, uint64_t toPoint);
    void                           signal(uint64_t point);

    int                            drmFD = -1;
    Hyprutils::OS::CFileDescriptor syncobjFd;
    uint32_t                       handle = 0;
    WP<CSyncTimeline>              self;

  private:
    CSyncTimeline() = default;

    std::vector<SP<SWaiter>> waiters;
};
