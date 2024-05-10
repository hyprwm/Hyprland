#pragma once

#include <wayland-server-protocol.h>
#include "../helpers/WLListener.hpp"
#include "../macros.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/Vector2D.hpp"
#include <vector>

constexpr size_t MAX_SERIAL_STORE_LEN = 100;

struct wlr_surface;
class CWLSeatResource;
class IPointer;
class IKeyboard;

class CSeatManager {
  public:
    CSeatManager();

    void     updateCapabilities(uint32_t capabilities); // in IHID caps

    void     setMouse(SP<IPointer> mouse);
    void     setKeyboard(SP<IKeyboard> keeb);

    void     setKeyboardFocus(wlr_surface* surf);
    void     sendKeyboardKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state);
    void     sendKeyboardMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

    void     setPointerFocus(wlr_surface* surf, const Vector2D& local);
    void     sendPointerMotion(uint32_t timeMs, const Vector2D& local);
    void     sendPointerButton(uint32_t timeMs, uint32_t key, wl_pointer_button_state state);
    void     sendPointerFrame();
    void     sendPointerAxis(uint32_t timeMs, wl_pointer_axis axis, double value, int32_t discrete, wl_pointer_axis_source source, wl_pointer_axis_relative_direction relative);

    void     sendTouchDown(wlr_surface* surf, uint32_t timeMs, int32_t id, const Vector2D& local);
    void     sendTouchUp(uint32_t timeMs, int32_t id);
    void     sendTouchMotion(uint32_t timeMs, int32_t id, const Vector2D& local);
    void     sendTouchFrame();
    void     sendTouchCancel();
    void     sendTouchShape(int32_t id, const Vector2D& shape);
    void     sendTouchOrientation(int32_t id, double angle);

    uint32_t nextSerial(SP<CWLSeatResource> seatResource);
    // pops the serial if it was valid, meaning it is consumed.
    bool                serialValid(SP<CWLSeatResource> seatResource, uint32_t serial);

    void                onSetCursor(SP<CWLSeatResource> seatResource, uint32_t serial, wlr_surface* surf, const Vector2D& hotspot);

    SP<CWLSeatResource> seatResourceForClient(wl_client* client);

    struct {
        wlr_surface*        keyboardFocus = nullptr;
        WP<CWLSeatResource> keyboardFocusResource;

        wlr_surface*        pointerFocus = nullptr;
        WP<CWLSeatResource> pointerFocusResource;

        wlr_surface*        touchFocus = nullptr;
        WP<CWLSeatResource> touchFocusResource;
    } state;

    struct SSetCursorEvent {
        wlr_surface* surf = nullptr;
        Vector2D     hotspot;
    };

    struct {
        CSignal keyboardFocusChange;
        CSignal pointerFocusChange;
        CSignal touchFocusChange;
        CSignal setCursor; // SSetCursorEvent
    } events;

    // do not write to directly, use set...
    WP<IPointer>  mouse;
    WP<IKeyboard> keyboard;

  private:
    struct SSeatResourceContainer {
        SSeatResourceContainer(SP<CWLSeatResource>);

        WP<CWLSeatResource>   resource;
        std::vector<uint32_t> serials; // old -> new

        struct {
            CHyprSignalListener destroy;
        } listeners;
    };

    std::vector<SP<SSeatResourceContainer>> seatResources;
    void                                    onNewSeatResource(SP<CWLSeatResource> resource);
    SP<SSeatResourceContainer>              containerForResource(SP<CWLSeatResource> seatResource);

    struct {
        CHyprSignalListener newSeatResource;
    } listeners;

    DYNLISTENER(keyboardSurfaceDestroy);
    DYNLISTENER(pointerSurfaceDestroy);
    DYNLISTENER(touchSurfaceDestroy);

    friend struct SSeatResourceContainer;
};

inline UP<CSeatManager> g_pSeatManager;
