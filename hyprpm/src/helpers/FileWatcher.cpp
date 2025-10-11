#include "FileWatcher.hpp"
#include "Colors.hpp"
#include <iostream>
#include <filesystem>
#include <print>
#include <unistd.h>
#include <sys/select.h>
#include <cstring>

CFileWatcher::CFileWatcher() : m_iNotifyFd(-1), m_bHasChanges(false) {
    m_iNotifyFd = inotify_init1(IN_NONBLOCK);
    if (m_iNotifyFd == -1) {
        std::println(stderr, "Failed to initialize inotify: {}", strerror(errno));
    }
}

CFileWatcher::~CFileWatcher() {
    if (m_iNotifyFd != -1) {
        for (const auto& [wd, path] : m_mWatchDescriptors) {
            inotify_rm_watch(m_iNotifyFd, wd);
        }
        close(m_iNotifyFd);
    }
}

void CFileWatcher::addWatchRecursive(const std::string& path) {
    if (!std::filesystem::exists(path))
        return;

    if (std::filesystem::is_directory(path)) {
        // Watch the directory itself
        int wd = inotify_add_watch(m_iNotifyFd, path.c_str(),
                                     IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);

        if (wd != -1) {
            m_mWatchDescriptors[wd] = path;
            m_mPathToWd[path] = wd;
        }

        // Recursively watch subdirectories
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_directory()) {
                    // Skip common build directories
                    const std::string dirname = entry.path().filename().string();
                    if (dirname == "build" || dirname == ".git" || dirname == "builddir")
                        continue;

                    addWatchRecursive(entry.path().string());
                }
            }
        } catch (...) {
            // Ignore permission errors, etc.
        }
    }
}

bool CFileWatcher::addWatch(const std::string& path) {
    if (m_iNotifyFd == -1)
        return false;

    addWatchRecursive(path);
    return !m_mWatchDescriptors.empty();
}

void CFileWatcher::removeWatch(const std::string& path) {
    auto it = m_mPathToWd.find(path);
    if (it != m_mPathToWd.end()) {
        inotify_rm_watch(m_iNotifyFd, it->second);
        m_mWatchDescriptors.erase(it->second);
        m_mPathToWd.erase(it);
    }
}

bool CFileWatcher::waitForEvents(int timeoutMs) {
    if (m_iNotifyFd == -1)
        return false;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_iNotifyFd, &fds);

    struct timeval tv;
    struct timeval* tvptr = nullptr;

    if (timeoutMs >= 0) {
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        tvptr = &tv;
    }

    int ret = select(m_iNotifyFd + 1, &fds, nullptr, nullptr, tvptr);

    if (ret > 0) {
        // Read events
        char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
        ssize_t len = read(m_iNotifyFd, buffer, sizeof(buffer));

        if (len > 0) {
            const struct inotify_event* event;
            for (char* ptr = buffer; ptr < buffer + len; ptr += sizeof(struct inotify_event) + event->len) {
                event = (const struct inotify_event*)ptr;

                // Check if it's a relevant file change (ignore directories and non-source files)
                if (event->len > 0) {
                    std::string filename = event->name;
                    // Only care about source files
                    if (filename.ends_with(".cpp") || filename.ends_with(".hpp") ||
                        filename.ends_with(".c") || filename.ends_with(".h") ||
                        filename.ends_with(".toml") || filename == "Makefile" ||
                        filename.ends_with(".cmake") || filename == "CMakeLists.txt") {
                        m_bHasChanges = true;
                    }
                }
            }
        }

        return m_bHasChanges;
    }

    return false;
}

bool CFileWatcher::hasChanges() {
    return m_bHasChanges;
}

void CFileWatcher::clearChanges() {
    m_bHasChanges = false;
}
