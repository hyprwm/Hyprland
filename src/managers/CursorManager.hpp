#pragma once

#include <string>
#include <hyprcursor/hyprcursor.hpp>
#include "../includes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../macros.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include "managers/XCursorManager.hpp"
#include <aquamarine/buffer/Buffer.hpp>

class CWLSurface;

AQUAMARINE_FORWARD(IBuffer);

class CCursorBuffer : public Aquamarine::IBuffer {
  public:
    CCursorBuffer(cairo_surface_t* surf, const Vector2D& size, const Vector2D& hotspot);
    CCursorBuffer(const uint8_t* pixelData, const Vector2D& size, const Vector2D& hotspot);
    ~CCursorBuffer() = default;

    virtual Aquamarine::eBufferCapability          caps();
    virtual Aquamarine::eBufferType                type();
    virtual void                                   update(const Hyprutils::Math::CRegion& damage);
    virtual bool                                   isSynchronous(); // whether the updates to this buffer are synchronous, aka happen over cpu
    virtual bool                                   good();
    virtual Aquamarine::SSHMAttrs                  shm();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();

  private:
    Vector2D             m_hotspot;
    std::vector<uint8_t> m_data;
    size_t               m_stride = 0;
};

class CCursorManager {
  public:
    CCursorManager();
    ~CCursorManager();

    SP<Aquamarine::IBuffer> getCursorBuffer();

    void                    setCursorFromName(const std::string& name);
    void                    setCursorSurface(SP<CWLSurface> surf, const Vector2D& hotspot);
    void                    setCursorBuffer(SP<CCursorBuffer> buf, const Vector2D& hotspot, const float& scale);
    void                    setAnimationTimer(const int& frame, const int& delay);

    bool                    changeTheme(const std::string& name, const int size);
    void                    updateTheme();
    SCursorImageData        dataFor(const std::string& name); // for xwayland
    void                    setXWaylandCursor();
    void                    syncGsettings();

    void                    tickAnimatedCursor();

  private:
    bool                               m_ourBufferConnected = false;
    std::vector<SP<CCursorBuffer>>     m_cursorBuffers;

    UP<Hyprcursor::CHyprcursorManager> m_hyprcursor;
    UP<CXCursorManager>                m_xcursor;
    SP<SXCursors>                      m_currentXcursor;

    std::string                        m_theme       = "";
    int                                m_size        = 0;
    float                              m_cursorScale = 1.0;

    Hyprcursor::SCursorStyleInfo       m_currentStyleInfo;

    SP<CEventLoopTimer>                m_animationTimer;
    int                                m_currentAnimationFrame = 0;
    Hyprcursor::SCursorShapeData       m_currentCursorShapeData;
};

inline UP<CCursorManager> g_pCursorManager;
