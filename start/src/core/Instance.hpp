#pragma once

#include <hyprutils/os/FileDescriptor.hpp>
#include <thread>

#include "../helpers/Memory.hpp"

class CHyprlandInstance {
  public:
    CHyprlandInstance()  = default;
    ~CHyprlandInstance() = default;

    CHyprlandInstance(const CHyprlandInstance&) = delete;
    CHyprlandInstance(CHyprlandInstance&)       = delete;
    CHyprlandInstance(CHyprlandInstance&&)      = delete;

    bool run(bool safeMode = false); // if returns false, restart.
    void forceQuit();

  private:
    void                           runHyprlandThread(bool safeMode);
    void                           clearFd(const Hyprutils::OS::CFileDescriptor& fd);
    void                           dispatchHyprlandEvent();

    int                            m_hlPid = -1;

    Hyprutils::OS::CFileDescriptor m_fromHlPid, m_toHlPid;
    Hyprutils::OS::CFileDescriptor m_wakeupRead, m_wakeupWrite;

    bool                           m_hyprlandInitialized = false;
    bool                           m_hyprlandExiting     = false;

    std::thread                    m_hlThread;
};

inline UP<CHyprlandInstance> g_instance;
