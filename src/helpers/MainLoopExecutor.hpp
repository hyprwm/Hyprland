#pragma once

#include <functional>
#include <hyprutils/os/FileDescriptor.hpp>
#include <wayland-server-core.h>

class CMainLoopExecutor {
  public:
    /*
        MainLoopExecutor

        Executes a function on the main thread once the writeFd() has some data written to it,
        then destroys itself.

        Needs to be kept owned, otherwise will die and kill the fds.
    */

    CMainLoopExecutor(std::function<void()>&& callback);
    ~CMainLoopExecutor();

    // Call from your worker thread: signals to the main thread. Destroy afterwards.
    void signal();

    // do not call
    void onFired();

  private:
    Hyprutils::OS::CFileDescriptor m_readFd, m_writeFd;
    wl_event_source*               m_event = nullptr;
    std::function<void()>          m_fn;
};
