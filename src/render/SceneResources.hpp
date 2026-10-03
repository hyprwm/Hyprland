#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/Format.hpp"
#include "../helpers/cm/ColorManagement.hpp"

namespace Render {
    class IFramebuffer;
    class ITexture;

    // The monitor adapter uses its existing authoritative flags. Isolated owners
    // retain allocations between sessions, but never reuse previous scene pixels.
    class CSceneResources {
      public:
        explicit CSceneResources(PHLMONITORREF monitor);
        explicit CSceneResources(SP<IFramebuffer> blurFramebuffer);
        ~CSceneResources();

        CSceneResources(const CSceneResources&)            = delete;
        CSceneResources& operator=(const CSceneResources&) = delete;

        bool             isolated() const;
        bool             prepare(const Vector2D& size, DRMFormat format, NColorManagement::PImageDescription description);
        SP<IFramebuffer> blurFramebuffer() const;
        SP<ITexture>     blurTexture() const;
        bool             canPrecomputeBlur() const;
        SP<IFramebuffer> prepareWorkBuffer(SP<IFramebuffer> framebuffer) const;
        bool             blurDirty() const;
        void             setBlurDirty(bool dirty);
        bool             blurQueued() const;
        void             setBlurQueued(bool queued);
        void             completePreBlur();

      private:
        PHLMONITORREF    m_monitor;
        SP<IFramebuffer> m_blurFramebuffer;
        bool             m_isolated = false;
        bool             m_dirty    = true;
        bool             m_queued   = false;
        bool             m_valid    = false;
        bool             m_prepared = false;
    };
}
