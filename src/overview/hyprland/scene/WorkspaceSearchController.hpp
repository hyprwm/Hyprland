#pragma once

#include "../../../desktop/DesktopTypes.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../helpers/math/Math.hpp"
#include "../../../helpers/signal/Signal.hpp"
#include "../../../helpers/time/Time.hpp"

#include <functional>
#include <string>

namespace Hyprtoolkit {
    class CTextboxElement;
    class IEmbeddedSurface;
}
namespace Render {
    class CRenderingContext;
    class IFramebuffer;
}

namespace Overview::Hyprland {
    class CWorkspaceSearchController {
      public:
        using FTextChanged = std::function<void(const std::string&)>;

        CWorkspaceSearchController();
        ~CWorkspaceSearchController();

        void        start(PHLMONITOR monitor, FTextChanged textChanged);
        void        reset();
        void        draw(Render::CRenderingContext& context, float opacity);

        bool        pointerMove(const Vector2D& monitorLocal);
        bool        pointerButton(uint32_t button, bool pressed, const Vector2D& monitorLocal);
        void        pointerLeave();
        void        keyboardKey(uint32_t keysym, bool down, bool repeat, std::string utf8, uint32_t modifiers);

        bool        contains(const Vector2D& monitorLocal) const;
        std::string query() const;

        void        setFocused(bool x);
        void        setQuery(const std::string& query);

      private:
        void                              updateGeometry();
        void                              damageMonitor() const;

        PHLMONITORREF                     m_monitor;
        SP<Hyprtoolkit::IEmbeddedSurface> m_surface;
        SP<Hyprtoolkit::CTextboxElement>  m_textbox;
        SP<Render::IFramebuffer>          m_framebuffer;
        FTextChanged                      m_textChanged;
        CBox                              m_logicalBox;
        CBox                              m_pixelBox;
        size_t                            m_pressedButtons = 0;
        bool                              m_pointerEntered = false;
        bool                              m_needsFrame     = true;
        bool                              m_fullRedraw     = true;
        bool                              m_focused        = false;

        struct {
            CHyprSignalListener frameRequested;
            CHyprSignalListener damaged;
            CHyprSignalListener cursorChanged;
        } m_listeners;
    };
}
