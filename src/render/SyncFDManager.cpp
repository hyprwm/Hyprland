#include "SyncFDManager.hpp"
#include <utility>

using namespace Render;

Hyprutils::OS::CFileDescriptor& ISyncFDManager::fd() {
    return m_fd;
}

Hyprutils::OS::CFileDescriptor&& ISyncFDManager::takeFd() {
    return std::move(m_fd);
}

bool ISyncFDManager::isValid() {
    return m_valid && m_fd.isValid();
}
