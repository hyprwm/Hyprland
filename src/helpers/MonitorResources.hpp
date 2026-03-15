#pragma once

#include "Monitor.hpp"
#include "Format.hpp"
#include "time/Timer.hpp"
#include "../render/Framebuffer.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <vector>

namespace Monitor {
    class CMonitorResources {
      public:
        CMonitorResources(WP<CMonitor> monitor, DRMFormat format, Vector2D size, NColorManagement::PImageDescription imageDescription);

        SP<Render::IFramebuffer> getUnusedWorkBuffer();
        void                     forEachUnusedFB(std::function<void(SP<Render::IFramebuffer>)> callback, bool includeNamed = false);
        bool                     hasMirrorFB();
        SP<Render::IFramebuffer> mirrorFB();

        SP<Render::ITexture>     m_stencilTex; // TODO fix blur ignore alpha and remove
        SP<Render::IFramebuffer> m_blurFB;

      private:
        void initFB(SP<Render::IFramebuffer> fb);

        struct SResource {
            SP<Render::IFramebuffer> buffer;
            CTimer                   lastUsed;
        };

        SP<Render::IFramebuffer>            m_monitorMirrorFB;
        WP<CMonitor>                        m_monitor;
        DRMFormat                           m_drmFormat;
        Vector2D                            m_size;
        NColorManagement::PImageDescription m_imageDescription;

        std::vector<SResource>              m_workBuffers;

        friend class ::CMonitor;
    };
}
