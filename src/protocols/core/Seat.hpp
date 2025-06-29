#pragma once

/*
    Implementations for:
     - wl_seat
     - wl_keyboard
     - wl_pointer
     - wl_touch
*/

#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include <wayland-server-protocol.h>
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/math/Math.hpp"
#include "../types/SurfaceRole.hpp"

constexpr const char* HL_SEAT_NAME = "Hyprland";

class IKeyboard;
class CWLSurfaceResource;

class CWLPointerResource;
class CWLKeyboardResource;
class CWLTouchResource;
class CWLSeatResource;

class CCursorSurfaceRole : public ISurfaceRole {
  public:
    virtual eSurfaceRole role() {
        return SURFACE_ROLE_CURSOR;
    }

    // gets the current pixel data from a shm surface
    // will assert if the surface is not a cursor
    static std::vector<uint8_t>& cursorPixelData(SP<CWLSurfaceResource> surface);

  private:
    std::vector<uint8_t> m_cursorShmPixelData;
};

class CWLTouchResource {
  public:
    CWLTouchResource(SP<CWlTouch> resource_, SP<CWLSeatResource> owner_);

    bool                good();
    void                sendDown(SP<CWLSurfaceResource> surface, uint32_t timeMs, int32_t id, const Vector2D& local);
    void                sendUp(uint32_t timeMs, int32_t id);
    void                sendMotion(uint32_t timeMs, int32_t id, const Vector2D& local);
    void                sendFrame();
    void                sendCancel();
    void                sendShape(int32_t id, const Vector2D& shape);
    void                sendOrientation(int32_t id, double angle);

    WP<CWLSeatResource> m_owner;

  private:
    SP<CWlTouch>           m_resource;
    WP<CWLSurfaceResource> m_currentSurface;

    int                    m_fingers = 0;

    struct {
        CHyprSignalListener destroySurface;
    } m_listeners;
};

class CWLPointerResource {
  public:
    CWLPointerResource(SP<CWlPointer> resource_, SP<CWLSeatResource> owner_);

    bool                good();
    int                 version();
    void                sendEnter(SP<CWLSurfaceResource> surface, const Vector2D& local);
    void                sendLeave();
    void                sendMotion(uint32_t timeMs, const Vector2D& local);
    void                sendButton(uint32_t timeMs, uint32_t button, wl_pointer_button_state state);
    void                sendAxis(uint32_t timeMs, wl_pointer_axis axis, double value);
    void                sendFrame();
    void                sendAxisSource(wl_pointer_axis_source source);
    void                sendAxisStop(uint32_t timeMs, wl_pointer_axis axis);
    void                sendAxisDiscrete(wl_pointer_axis axis, int32_t discrete);
    void                sendAxisValue120(wl_pointer_axis axis, int32_t value120);
    void                sendAxisRelativeDirection(wl_pointer_axis axis, wl_pointer_axis_relative_direction direction);

    WP<CWLSeatResource> m_owner;

  private:
    SP<CWlPointer>         m_resource;
    WP<CWLSurfaceResource> m_currentSurface;

    std::vector<uint32_t>  m_pressedButtons;

    struct {
        CHyprSignalListener destroySurface;
    } m_listeners;
};

class CWLKeyboardResource {
  public:
    CWLKeyboardResource(SP<CWlKeyboard> resource_, SP<CWLSeatResource> owner_);

    bool                good();
    void                sendKeymap(SP<IKeyboard> keeb);
    void                sendEnter(SP<CWLSurfaceResource> surface);
    void                sendLeave();
    void                sendKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state);
    void                sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);
    void                repeatInfo(uint32_t rate, uint32_t delayMs);

    WP<CWLSeatResource> m_owner;

  private:
    SP<CWlKeyboard>        m_resource;
    WP<CWLSurfaceResource> m_currentSurface;

    struct {
        CHyprSignalListener destroySurface;
    } m_listeners;

    std::string m_lastKeymap  = "<none>";
    uint32_t    m_lastRate    = 0;
    uint32_t    m_lastDelayMs = 0;
};

class CWLSeatResource {
  public:
    CWLSeatResource(SP<CWlSeat> resource_);
    ~CWLSeatResource();

    void                                 sendCapabilities(uint32_t caps); // uses IHID capabilities

    bool                                 good();
    wl_client*                           client();

    std::vector<WP<CWLPointerResource>>  m_pointers;
    std::vector<WP<CWLKeyboardResource>> m_keyboards;
    std::vector<WP<CWLTouchResource>>    m_touches;

    WP<CWLSeatResource>                  m_self;

    struct {
        CSignalT<> destroy;
    } m_events;

  private:
    SP<CWlSeat> m_resource;
    wl_client*  m_client = nullptr;
};

class CWLSeatProtocol : public IWaylandProtocol {
  public:
    CWLSeatProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    struct {
        CSignalT<SP<CWLSeatResource>> newSeatResource;
    } m_events;

  private:
    void updateCapabilities(uint32_t caps); // in IHID caps
    void updateRepeatInfo(uint32_t rate, uint32_t delayMs);
    void updateKeymap();

    void destroyResource(CWLSeatResource* resource);
    void destroyResource(CWLKeyboardResource* resource);
    void destroyResource(CWLTouchResource* resource);
    void destroyResource(CWLPointerResource* resource);

    //
    std::vector<SP<CWLSeatResource>>     m_seatResources;
    std::vector<SP<CWLKeyboardResource>> m_keyboards;
    std::vector<SP<CWLTouchResource>>    m_touches;
    std::vector<SP<CWLPointerResource>>  m_pointers;

    SP<CWLSeatResource>                  seatResourceForClient(wl_client* client);

    //
    uint32_t m_currentCaps = 0;

    friend class CWLSeatResource;
    friend class CWLKeyboardResource;
    friend class CWLTouchResource;
    friend class CWLPointerResource;
    friend class CSeatManager;
};

namespace PROTO {
    inline UP<CWLSeatProtocol> seat;
};
