#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <sys/inotify.h>

class CFileWatcher {
  public:
    CFileWatcher();
    ~CFileWatcher();

    // Add a directory to watch (recursive)
    bool addWatch(const std::string& path);

    // Remove a watch
    void removeWatch(const std::string& path);

    // Wait for events with timeout (returns true if events occurred)
    bool waitForEvents(int timeoutMs = -1);

    // Check if any watched files have changed since last check
    bool hasChanges();

    // Clear the change flag
    void clearChanges();

  private:
    int                                    m_iNotifyFd;
    std::unordered_map<int, std::string>  m_mWatchDescriptors;
    std::unordered_map<std::string, int>  m_mPathToWd;
    bool                                   m_bHasChanges;

    void addWatchRecursive(const std::string& path);
};
