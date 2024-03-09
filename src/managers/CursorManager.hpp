#pragma once

#include <string>
#include <hyprcursor/hyprcursor.hpp>
#include <memory>
#include "../includes.hpp"
#include "../helpers/Vector2D.hpp"

struct wlr_buffer;
struct wlr_xcursor_manager;
struct wlr_xwayland;

class CCursorManager {
  public:
    CCursorManager();

    wlr_buffer*      getCursorBuffer();

    void             setCursorFromName(const std::string& name);
    void             setCursorSurface(wlr_surface* surf, const Vector2D& hotspot);

    void             changeTheme(const std::string& name, const int size);
    void             updateTheme();
    SCursorImageData dataFor(const std::string& name); // for xwayland
    void             setXWaylandCursor(wlr_xwayland* xwayland);

    void             tickAnimatedCursor();

    class CCursorBuffer {
      public:
        CCursorBuffer(cairo_surface_t* surf, const Vector2D& size, const Vector2D& hotspot);
        ~CCursorBuffer();

        struct SCursorWlrBuffer {
            wlr_buffer       base;
            cairo_surface_t* surface = nullptr;
            bool             dropped = false;
            CCursorBuffer*   parent  = nullptr;
        } wlrBuffer;

      private:
        Vector2D size;
        Vector2D hotspot;

        friend class CCursorManager;
    };

    void dropBufferRef(CCursorBuffer* ref);

    bool m_bOurBufferConnected = false;

  private:
    std::vector<std::unique_ptr<CCursorBuffer>>     m_vCursorBuffers;

    std::unique_ptr<Hyprcursor::CHyprcursorManager> m_pHyprcursor;

    std::string                                     m_szTheme      = "";
    int                                             m_iSize        = 24;
    float                                           m_fCursorScale = 1.0;

    Hyprcursor::SCursorStyleInfo                    m_sCurrentStyleInfo;

    wl_event_source*                                m_pAnimationTimer        = nullptr;
    int                                             m_iCurrentAnimationFrame = 0;
    Hyprcursor::SCursorShapeData                    m_sCurrentCursorShapeData;

    // xcursor fallback
    wlr_xcursor_manager* m_pWLRXCursorMgr = nullptr;
};

inline std::unique_ptr<CCursorManager> g_pCursorManager;