#pragma once

#include "types.hpp"
#include "pass/Pass.hpp"

namespace Aquamarine {
    class IBuffer;
}

struct SBackdropScope;

namespace Render {
    class IRenderbuffer;

    // One reusable render session. Only m_data may be copied for nested draws.
    class CRenderContext {
      public:
        CRenderContext();
        ~CRenderContext();
        CRenderContext(const CRenderContext&)               = delete;
        CRenderContext& operator=(const CRenderContext&)    = delete;
        CRenderContext(CRenderContext&&)                    = delete;
        CRenderContext&         operator=(CRenderContext&&) = delete;

        bool                    begin();
        bool                    active() const;
        void                    reset();

        SRenderData             m_data;
        CRenderPass             m_pass;
        CRenderPass*            m_currentPass = nullptr;
        eRenderMode             m_mode        = RENDER_MODE_NORMAL;
        SP<Aquamarine::IBuffer> m_currentBuffer;
        SP<IRenderbuffer>       m_currentRenderbuffer;

        struct SBackdropCapture {
            SP<SBackdropScope> scope;
            SP<IFramebuffer>   framebuffer;
        };
        std::vector<SBackdropCapture> m_backdropCaptures;

        struct SCMSettingsCacheEntry {
            uint64_t    srcDescId = 0, dstDescId = 0;
            void*       surfacePtr      = nullptr; // read-only!!
            bool        modifySDR       = false;
            float       sdrMinLuminance = -1.F;
            int         sdrMaxLuminance = -1;
            SCMSettings settings;
        };
        std::vector<SCMSettingsCacheEntry> m_cmSettingsCache;

        bool                               m_blockSurfaceFeedback = false;
        bool                               m_renderingSnapshot    = false;
        bool                               m_swapchainAcquired    = false;

        struct {
            bool fakeFrame            = false;
            bool offloadedFramebuffer = false;
            bool applyFinalShader     = false;
        } m_gl;

      private:
        bool m_active = false;
    };
}
