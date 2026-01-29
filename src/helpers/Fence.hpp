#pragma once

#include "helpers/time/Time.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CFence {
  public:
    CFence(int fd = -1);
    CFence(std::array<int, 4> fds);
    ~CFence()                                                          = default;
    CFence(const CFence&)                                              = delete;
    CFence& operator=(const CFence&)                                   = delete;
    CFence(CFence&&) noexcept                                          = default;
    CFence&                               operator=(CFence&&) noexcept = default;
    bool                                  isValid();
    const Hyprutils::OS::CFileDescriptor& fd();
    void                                  setDeadline(const Time::steady_tp& deadline);
    void                                  merge(Hyprutils::OS::CFileDescriptor&& fence);

  private:
    int                            doIoctl(int fd, unsigned long request, void* arg);
    Hyprutils::OS::CFileDescriptor m_fence;
};
