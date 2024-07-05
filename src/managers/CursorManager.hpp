#pragma once

#include <string>
#include <hyprcursor/hyprcursor.hpp>
#include <memory>
#include "../includes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"

class CWLSurface;

AQUAMARINE_FORWARD(IBuffer);

class CCursorManager {
  public:
    CCursorManager();
    ~CCursorManager();

    SP<Aquamarine::IBuffer> getCursorBuffer();

    void                    setCursorFromName(const std::string& name);
    void                    setCursorSurface(SP<CWLSurface> surf, const Vector2D& hotspot);
    void                    setXCursor(const std::string& name);

    bool                    changeTheme(const std::string& name, const int size);
    void                    updateTheme();
    SCursorImageData        dataFor(const std::string& name); // for xwayland
    void                    setXWaylandCursor();

    void                    tickAnimatedCursor();

    class CCursorBuffer : public Aquamarine::IBuffer {
      public:
        CCursorBuffer(cairo_surface_t* surf, const Vector2D& size, const Vector2D& hotspot);
        CCursorBuffer(uint8_t* pixelData, const Vector2D& size, const Vector2D& hotspot);
        ~CCursorBuffer();

        virtual Aquamarine::eBufferCapability          caps();
        virtual Aquamarine::eBufferType                type();
        virtual void                                   update(const Hyprutils::Math::CRegion& damage);
        virtual bool                                   isSynchronous(); // whether the updates to this buffer are synchronous, aka happen over cpu
        virtual bool                                   good();
        virtual Aquamarine::SSHMAttrs                  shm();
        virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
        virtual void                                   endDataPtr();

      private:
        Vector2D         hotspot;
        cairo_surface_t* surface   = nullptr;
        uint8_t*         pixelData = nullptr;
        size_t           stride    = 0;

        friend class CCursorManager;
    };

    void dropBufferRef(CCursorBuffer* ref);

    bool m_bOurBufferConnected = false;

  private:
    std::vector<SP<CCursorBuffer>>                  m_vCursorBuffers;

    std::unique_ptr<Hyprcursor::CHyprcursorManager> m_pHyprcursor;

    std::string                                     m_szTheme      = "";
    int                                             m_iSize        = 0;
    float                                           m_fCursorScale = 1.0;

    Hyprcursor::SCursorStyleInfo                    m_sCurrentStyleInfo;

    wl_event_source*                                m_pAnimationTimer        = nullptr;
    int                                             m_iCurrentAnimationFrame = 0;
    Hyprcursor::SCursorShapeData                    m_sCurrentCursorShapeData;

    // gangsta bootleg XCursor impl. Whenever Hyprland has to use
    // an xcursor, just use the pointer.
    struct SXCursor {
        Vector2D             size;
        Vector2D             hotspot;
        std::vector<uint8_t> pixels;
    };

    struct SXCursorManager {
        void         loadTheme(const std::string& name, int size);

        bool         themeLoaded = false;
        std::string  themeName   = "";
        SP<SXCursor> defaultCursor;
    } xcursor;
};

inline std::unique_ptr<CCursorManager> g_pCursorManager;