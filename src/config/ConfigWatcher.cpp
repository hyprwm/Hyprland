#include "ConfigWatcher.hpp"
#include <sys/inotify.h>
#include "../debug/Log.hpp"
#include <algorithm>
#include <ranges>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>

using namespace Hyprutils::OS;

CConfigWatcher::CConfigWatcher() : m_inotifyFd(inotify_init()) {
    if (!m_inotifyFd.isValid()) {
        Debug::log(ERR, "CConfigWatcher couldn't open an inotify node. Config will not be automatically reloaded");
        return;
    }

    // TODO: make CFileDescriptor take F_GETFL, F_SETFL
    const int FLAGS = fcntl(m_inotifyFd.get(), F_GETFL, 0);
    if (fcntl(m_inotifyFd.get(), F_SETFL, FLAGS | O_NONBLOCK) < 0) {
        Debug::log(ERR, "CConfigWatcher couldn't non-block inotify node. Config will not be automatically reloaded");
        m_inotifyFd.reset();
        return;
    }
}

CFileDescriptor& CConfigWatcher::getInotifyFD() {
    return m_inotifyFd;
}

void CConfigWatcher::setWatchList(const std::vector<std::string>& paths) {

    // we clear all watches first, because whichever fired is now invalid
    // or that is at least what it seems to be.
    // since we don't know which fired,
    // plus it doesn't matter that much, these ops are done rarely and fast anyways.

    // cleanup old paths
    for (auto& watch : m_watches) {
        inotify_rm_watch(m_inotifyFd.get(), watch.wd);
    }

    m_watches.clear();
    m_symlinks.clear();

    // add new paths
    for (const auto& path : paths) {
        m_watches.emplace_back(SInotifyWatch{
            .wd   = inotify_add_watch(m_inotifyFd.get(), path.c_str(), IN_MODIFY | IN_DONT_FOLLOW),
            .file = path,
        });

        std::error_code ec, ec2;
        const auto      CANONICAL  = std::filesystem::canonical(path, ec);
        const auto      IS_SYMLINK = std::filesystem::is_symlink(path, ec2);
        if (!ec && !ec2 && IS_SYMLINK) {
            Debug::log(INFO, "Found symlink {} -> {}", path, CANONICAL.c_str());
            m_symlinks.insert_or_assign(path, CANONICAL.c_str());
            m_watches.emplace_back(SInotifyWatch{
                .wd   = inotify_add_watch(m_inotifyFd.get(), CANONICAL.c_str(), IN_MODIFY),
                .file = CANONICAL.c_str(),
            });
        }
    }
}

void CConfigWatcher::setOnChange(const std::function<void(const SConfigWatchEvent&)>& fn) {
    m_watchCallback = fn;
}

void CConfigWatcher::onInotifyEvent() {
    inotify_event ev;
    while (read(m_inotifyFd.get(), &ev, sizeof(ev)) > 0) {
        const auto WD = std::ranges::find_if(m_watches.begin(), m_watches.end(), [wd = ev.wd](const auto& e) { return e.wd == wd; });

        if (WD == m_watches.end()) {
            Debug::log(ERR, "CConfigWatcher: got an event for wd {} which we don't have?!", ev.wd);
            return;
        }

        std::error_code ec, ec2;
        const auto      CANONICAL  = std::filesystem::canonical(WD->file, ec);
        const auto      IS_SYMLINK = std::filesystem::is_symlink(WD->file, ec2);

        if (IS_SYMLINK) {
            const auto prev_symlink       = m_symlinks.at(WD->file);
            const auto prev_symlink_watch = std::ranges::find_if(m_watches, [prev_symlink](const auto& e) { return e.file == prev_symlink; });

            Debug::log(INFO, "Path {} -> {} got event", WD->file, prev_symlink);
            if (!ec && !ec2 && IS_SYMLINK && CANONICAL != prev_symlink) {
                Debug::log(INFO, "Path {} changed to {}", WD->file, CANONICAL.c_str());
                Debug::log(INFO, "Removing {} from m_symlinks and m_watches", prev_symlink);
                m_symlinks.insert_or_assign(WD->file, CANONICAL);
                const auto [begin, end] = std::ranges::remove_if(m_watches, [prev_symlink](const auto& e) { return e.file == prev_symlink; });
                m_watches.erase(begin, end);

                Debug::log(INFO, "Removing {} inotify watch", prev_symlink);
                inotify_rm_watch(m_inotifyFd.get(), prev_symlink_watch->wd);

                Debug::log(INFO, "Adding {} inotify watch", CANONICAL.c_str());
                m_watches.emplace_back(SInotifyWatch{
                    .wd   = inotify_add_watch(m_inotifyFd.get(), CANONICAL.c_str(), IN_MODIFY),
                    .file = CANONICAL.c_str(),
                });
            }
        }

        m_watchCallback(SConfigWatchEvent{
            .file = WD->file,
        });
    }
}
