#pragma once

#include "Monitor.hpp"
#include "../helpers/Format.hpp"
#include "../helpers/time/Timer.hpp"
#include "../render/Framebuffer.hpp"
#include "../render/scene/SceneStack.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <vector>

namespace Monitor {
    class CMonitorResources {
      public:
        CMonitorResources(WP<CMonitor> monitor, DRMFormat format, Vector2D size, NColorManagement::PImageDescription imageDescription);

        SP<Render::IFramebuffer> getUnusedWorkBuffer();
        SP<Render::IFramebuffer> getUnusedWorkBuffer(const Vector2D& size);
        void                     forEachUnusedFB(std::function<void(SP<Render::IFramebuffer>)> callback, bool includeNamed = false);
        bool                     hasMirrorFB() const;
        bool                     shouldKeepMirrorFB() const;
        void                     releaseMirrorFB();
        void                     invalidateMirrorFB();
        void                     markMirrorFBStale(const CRegion& damage);
        void                     markMirrorFBStale();
        void                     markMirrorFBUpdated();
        CRegion                  pendingMirrorFBDamage() const;
        void                     enableMirror();
        void                     disableMirror();
        SP<Render::IFramebuffer> mirrorFB();
        SP<Render::ITexture>     getMirrorTexture();
        void                     refreshBlurFB();
        SP<Render::ITexture>     m_mirrorTex;

        SP<Render::ITexture>     m_stencilTex; // TODO fix blur ignore alpha and remove
        SP<Render::IFramebuffer> m_blurFB;

        Render::CSceneStack      m_sceneStack;

      private:
        void                                initFB(SP<Render::IFramebuffer> fb);
        void                                setImageDescription(NColorManagement::PImageDescription imageDescription);
        NColorManagement::PImageDescription getMirrorTexImageDescription();
        Vector2D                            mirrorFBDamageSize() const;

        struct SResource {
            SP<Render::IFramebuffer> buffer;
            CTimer                   lastUsed;
        };

        SP<Render::IFramebuffer>            m_monitorMirrorFB;
        CRegion                             m_mirrorFBStaleDamage;
        WP<CMonitor>                        m_monitor;
        DRMFormat                           m_drmFormat;
        Vector2D                            m_size;
        NColorManagement::PImageDescription m_imageDescription;
        bool                                m_mirrorFBValid            = false;
        bool                                m_mirrorFBNeedsFullRefresh = true;

        std::vector<SResource>              m_workBuffers;
        std::vector<SResource>              m_sizedWorkBuffers;

        friend class CMonitor;
    };
}
