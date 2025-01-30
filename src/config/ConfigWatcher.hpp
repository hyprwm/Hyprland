#pragma once
#include "../helpers/memory/Memory.hpp"
#include <vector>
#include <string>
#include <functional>
#include <hyprutils/os/FileDescriptor.hpp>

class CConfigWatcher {
  public:
    CConfigWatcher();
    ~CConfigWatcher() = default;

    struct SConfigWatchEvent {
        std::string file;
    };

    Hyprutils::OS::CFileDescriptor& getInotifyFD();
    void                            setWatchList(const std::vector<std::string>& paths);
    void                            setOnChange(const std::function<void(const SConfigWatchEvent&)>& fn);
    void                            onInotifyEvent();

  private:
    struct SInotifyWatch {
        int         wd = -1;
        std::string file;
    };

    std::function<void(const SConfigWatchEvent&)> m_watchCallback;
    std::vector<SInotifyWatch>                    m_watches;
    Hyprutils::OS::CFileDescriptor                m_inotifyFd;
};

inline UP<CConfigWatcher> g_pConfigWatcher = makeUnique<CConfigWatcher>();
