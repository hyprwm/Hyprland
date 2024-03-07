#pragma once

#include <string>
#include <hyprcursor/hyprcursor.hpp>
#include <memory>
#include "../includes.hpp"
#include "../helpers/Vector2D.hpp"

struct wlr_buffer;

class CCursorManager {
  public:
    CCursorManager();

    wlr_buffer*      getCursorBuffer();

    void             setCursorFromName(const std::string& name);
    void             setCursorSurface(wlr_surface* surf, const Vector2D& hotspot);

    void             changeTheme(const std::string& name, const int size);
    void             updateTheme();
    SCursorImageData dataFor(const std::string& name); // for xwayland

    class CCursorBuffer {
      public:
        CCursorBuffer(cairo_surface_t* surf, const Vector2D& size, const Vector2D& hotspot);
        ~CCursorBuffer();

        struct SCursorWlrBuffer {
            wlr_buffer       base;
            cairo_surface_t* surface = nullptr;
            bool             dropped = false;
        } wlrBuffer;

      private:
        Vector2D size;
        Vector2D hotspot;

        friend class CCursorManager;
    };

    bool m_bOurBufferConnected = false;

  private:
    std::unique_ptr<CCursorBuffer>                  m_sCursorBuffer;

    std::unique_ptr<Hyprcursor::CHyprcursorManager> m_pHyprcursor;

    std::string                                     m_szTheme      = "";
    int                                             m_iSize        = 24;
    float                                           m_fCursorScale = 1.0;

    Hyprcursor::SCursorStyleInfo                    m_sCurrentStyleInfo;
};

inline std::unique_ptr<CCursorManager> g_pCursorManager;