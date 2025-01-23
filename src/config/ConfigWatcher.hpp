#pragma once
#include "../helpers/memory/Memory.hpp"
#include <vector>
#include <string>
#include <functional>

class CConfigWatcher {
  public:
    CConfigWatcher();
    ~CConfigWatcher();

    struct SConfigWatchEvent {
        std::string file;
    };

    int  getInotifyFD();
    void setWatchList(const std::vector<std::string>& paths);
    void setOnChange(const std::function<void(const SConfigWatchEvent&)>& fn);
    void onInotifyEvent();

  private:
    struct SInotifyWatch {
        int         wd = -1;
        std::string file;
    };

    std::function<void(const SConfigWatchEvent&)> m_watchCallback;
    std::vector<SInotifyWatch>                    m_watches;
    int                                           m_inotifyFd = -1;
};

inline UP<CConfigWatcher> g_pConfigWatcher = makeUnique<CConfigWatcher>();