#include "BufferReleaseManager.hpp"
#include "../helpers/Monitor.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include "render/OpenGL.hpp"

void CBufferReleaseManager::add(PHLMONITORREF monitor, const CHLBufferReference& buf) {
    if (!monitor)
        return;

    auto it = std::ranges::find_if(m_buffers[monitor], [&buf](auto& b) { return b == buf; });

    if (it != m_buffers[monitor].end())
        return;

    m_buffers[monitor].emplace_back(buf);
}

void CBufferReleaseManager::pageFlip(PHLMONITORREF monitor) {
    if (g_pHyprOpenGL->explicitSyncSupported() && monitor->m_inFence.isValid()) {
        for (auto& b : m_buffers[monitor]) {
            if (!b->m_syncReleasers.empty()) {
                for (auto& releaser : b->m_syncReleasers) {
                    releaser->addSyncFileFd(monitor->m_inFence);
                }
            }
        }

        std::erase_if(m_buffers[monitor], [](auto& b) { return !b->m_syncReleasers.empty(); });

        g_pEventLoopManager->doOnReadable(monitor->m_inFence.duplicate(), [buffers = std::move(m_buffers[monitor])] mutable { buffers.clear(); });
    } else {
        // release all buffer refs and hope implicit sync works
        m_buffers[monitor].clear();
    }
}

void CBufferReleaseManager::destroy(PHLMONITORREF monitor) {
    m_buffers.erase(monitor);
}
