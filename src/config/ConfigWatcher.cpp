#include "ConfigWatcher.hpp"
#if defined(__linux__)
#include <linux/limits.h>
#endif
#include <sys/inotify.h>
#include "../debug/Log.hpp"
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
            m_watches.emplace_back(SInotifyWatch{
                .wd   = inotify_add_watch(m_inotifyFd.get(), CANONICAL.c_str(), IN_MODIFY),
                .file = path,
            });
        }
    }
}

void CConfigWatcher::setOnChange(const std::function<void(const SConfigWatchEvent&)>& fn) {
    m_watchCallback = fn;
}

void CConfigWatcher::onInotifyEvent() {
    constexpr size_t                                     BUFFER_SIZE = sizeof(inotify_event) + NAME_MAX + 1;
    alignas(inotify_event) std::array<char, BUFFER_SIZE> buffer      = {};
    const ssize_t                                        bytesRead   = read(m_inotifyFd.get(), buffer.data(), buffer.size());
    if (bytesRead <= 0)
        return;

    for (size_t offset = 0; offset < sc<size_t>(bytesRead);) {
        const auto* ev = rc<const inotify_event*>(buffer.data() + offset);

        if (offset + sizeof(inotify_event) > sc<size_t>(bytesRead)) {
            Debug::log(ERR, "CConfigWatcher: malformed inotify event, truncated header");
            break;
        }

        if (offset + sizeof(inotify_event) + ev->len > sc<size_t>(bytesRead)) {
            Debug::log(ERR, "CConfigWatcher: malformed inotify event, truncated name field");
            break;
        }

        const auto WD = std::ranges::find_if(m_watches, [wd = ev->wd](const auto& e) { return e.wd == wd; });

        if (WD == m_watches.end())
            Debug::log(ERR, "CConfigWatcher: got an event for wd {} which we don't have?!", ev->wd);
        else
            m_watchCallback(SConfigWatchEvent{
                .file = WD->file,
            });

        offset += sizeof(inotify_event) + ev->len;
    }
}
