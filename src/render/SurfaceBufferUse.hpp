#pragma once

#include "../protocols/types/Buffer.hpp"

namespace Render {
    struct SSurfaceBufferUse {
        WP<CWLSurfaceResource> surface;
        CHLBufferReference     buffer;
    };

    void addSurfaceBufferUse(std::vector<SSurfaceBufferUse>& uses, WP<CWLSurfaceResource> surface, const CHLBufferReference& buffer);
    void mergeSurfaceBufferUses(std::vector<SSurfaceBufferUse>& pending, std::vector<SSurfaceBufferUse>& uses);
    bool attachSurfaceBufferReleaseFences(std::vector<SSurfaceBufferUse>& uses, const Hyprutils::OS::CFileDescriptor& fd);
    void releaseSurfaceBuffersOnReadable(Hyprutils::OS::CFileDescriptor fd, std::vector<SSurfaceBufferUse> uses, std::function<void()> callback);
}
