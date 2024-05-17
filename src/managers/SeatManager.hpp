#pragma once

#include <wayland-server-protocol.h>
#include "../helpers/WLListener.hpp"
#include "../macros.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/Vector2D.hpp"
#include "../protocols/types/DataDevice.hpp"
#include <vector>

constexpr size_t MAX_SERIAL_STORE_LEN = 100;

struct wlr_surface;
class CWLSeatResource;
class IPointer;
class IKeyboard;

/*
    A seat grab defines a restricted set of surfaces that can be focused.
    Only one grab can be active at a time

    when a grab is removed, refocus() will happen

    Different from a constraint.

    When first set with setGrab, SeatManager will try to find a surface that is at the mouse pointer to focus,
    from first added to last added. If none are, first is focused.
*/
class CSeatGrab {
  public:
    bool accepts(wlr_surface* surf);
    void add(wlr_surface* surf);
    void remove(wlr_surface* surf);
    void setCallback(std::function<void()> onEnd_);
    void clear();

    bool keyboard = false;
    bool pointer  = false;
    bool touch    = false;

    bool removeOnInput = true; // on hard input e.g. click outside, remove

  private:
    std::vector<wlr_surface*> surfs; // read-only
    std::function<void()>     onEnd;
    friend class CSeatManager;
};

class CSeatManager {
  public:
    CSeatManager();

    void     updateCapabilities(uint32_t capabilities); // in IHID caps

    void     setMouse(SP<IPointer> mouse);
    void     setKeyboard(SP<IKeyboard> keeb);
    void     updateActiveKeyboardData(); // updates the clients with the keymap and repeat info

    void     setKeyboardFocus(wlr_surface* surf);
    void     sendKeyboardKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state);
    void     sendKeyboardMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

    void     setPointerFocus(wlr_surface* surf, const Vector2D& local);
    void     sendPointerMotion(uint32_t timeMs, const Vector2D& local);
    void     sendPointerButton(uint32_t timeMs, uint32_t key, wl_pointer_button_state state);
    void     sendPointerFrame();
    void     sendPointerFrame(WP<CWLSeatResource> pResource);
    void     sendPointerAxis(uint32_t timeMs, wl_pointer_axis axis, double value, int32_t discrete, wl_pointer_axis_source source, wl_pointer_axis_relative_direction relative);

    void     sendTouchDown(wlr_surface* surf, uint32_t timeMs, int32_t id, const Vector2D& local);
    void     sendTouchUp(uint32_t timeMs, int32_t id);
    void     sendTouchMotion(uint32_t timeMs, int32_t id, const Vector2D& local);
    void     sendTouchFrame();
    void     sendTouchCancel();
    void     sendTouchShape(int32_t id, const Vector2D& shape);
    void     sendTouchOrientation(int32_t id, double angle);

    void     resendEnterEvents();

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

    struct {
        WP<IDataSource>     currentSelection;
        CHyprSignalListener destroySelection;
        WP<IDataSource>     currentPrimarySelection;
        CHyprSignalListener destroyPrimarySelection;
    } selection;

    void setCurrentSelection(SP<IDataSource> source);
    void setCurrentPrimarySelection(SP<IDataSource> source);

    // do not write to directly, use set...
    WP<IPointer>  mouse;
    WP<IKeyboard> keyboard;

    void          setGrab(SP<CSeatGrab> grab); // nullptr removes
    SP<CSeatGrab> seatGrab;

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

    void                                    refocusGrab();

    struct {
        CHyprSignalListener newSeatResource;
    } listeners;

    Vector2D lastLocalCoords;

    DYNLISTENER(keyboardSurfaceDestroy);
    DYNLISTENER(pointerSurfaceDestroy);
    DYNLISTENER(touchSurfaceDestroy);

    friend struct SSeatResourceContainer;
    friend class CSeatGrab;
};

inline UP<CSeatManager> g_pSeatManager;
