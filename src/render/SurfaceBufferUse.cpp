#include "SurfaceBufferUse.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../helpers/sync/SyncReleaser.hpp"

#include <algorithm>

using namespace Render;

void Render::addSurfaceBufferUse(std::vector<SSurfaceBufferUse>& uses, WP<CWLSurfaceResource> surface, const CHLBufferReference& buffer) {
    if (std::ranges::none_of(uses, [&](const auto& use) { return use.surface == surface && use.buffer == buffer; }))
        uses.push_back({.surface = surface, .buffer = buffer});
}

void Render::mergeSurfaceBufferUses(std::vector<SSurfaceBufferUse>& pending, std::vector<SSurfaceBufferUse>& uses) {
    if (pending.empty()) {
        pending.swap(uses);
        return;
    }

    for (const auto& use : uses)
        addSurfaceBufferUse(pending, use.surface, use.buffer);
    uses.clear();
}

bool Render::attachSurfaceBufferReleaseFences(std::vector<SSurfaceBufferUse>& uses, const Hyprutils::OS::CFileDescriptor& fd) {
    for (const auto& use : uses) {
        for (const auto& releaser : use.buffer->m_syncReleasers) {
            if (!releaser->addSyncFileFd(fd))
                return false; // Keep every lock until the caller synchronizes the GPU.
        }
    }

    // Release points now own synchronization, even if the surface has disappeared.
    std::erase_if(uses, [](const auto& use) { return !use.buffer->m_syncReleasers.empty(); });
    return true;
}

void Render::releaseSurfaceBuffersOnReadable(Hyprutils::OS::CFileDescriptor fd, std::vector<SSurfaceBufferUse> uses, std::function<void()> callback) {
    g_pEventLoopManager->doOnReadable(std::move(fd), [buffers = std::move(uses), callback = std::move(callback)]() mutable {
        buffers.clear();
        if (callback)
            callback();
    });
}
