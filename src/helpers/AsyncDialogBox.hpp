#pragma once

#include "../macros.hpp"
#include "./memory/Memory.hpp"
#include "./defer/Promise.hpp"

#include <vector>
#include <functional>

#include <hyprutils/os/Process.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

struct wl_event_source;

class CAsyncDialogBox {
  public:
    static SP<CAsyncDialogBox> create(const std::string& title, const std::string& description, std::vector<std::string> buttons);
    static bool                isAsyncDialogBox(pid_t pid);

    CAsyncDialogBox(const CAsyncDialogBox&)                     = delete;
    CAsyncDialogBox(CAsyncDialogBox&&)                          = delete;
    CAsyncDialogBox&          operator=(const CAsyncDialogBox&) = delete;
    CAsyncDialogBox&          operator=(CAsyncDialogBox&&)      = delete;

    SP<CPromise<std::string>> open();
    void                      kill();
    bool                      isRunning() const;

    void                      onWrite(int fd, uint32_t mask);

  private:
    CAsyncDialogBox(const std::string& title, const std::string& description, std::vector<std::string> buttons);

    pid_t                             m_dialogPid       = 0;
    wl_event_source*                  m_readEventSource = nullptr;
    Hyprutils::OS::CFileDescriptor    m_pipeReadFd;
    std::string                       m_stdout = "";

    const std::string                 m_title;
    const std::string                 m_description;
    const std::vector<std::string>    m_buttons;

    SP<CPromiseResolver<std::string>> m_promiseResolver;

    // WARNING: cyclic reference. This will be removed once the event source is removed to avoid dangling pointers
    SP<CAsyncDialogBox> m_selfReference;
    WP<CAsyncDialogBox> m_selfWeakReference;
};