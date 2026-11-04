#pragma once

#include <cstdint>
#include <wayland-server-protocol.h>
#include "../helpers/signal/Signal.hpp"
#include "../helpers/math/Math.hpp"
#include "../protocols/types/DataDevice.hpp"
#include "../devices/IKeyboard.hpp"
#include <functional>
#include <vector>

constexpr size_t MAX_SERIAL_STORE_LEN = 100;

class CWLSurfaceResource;
class CWLSeatResource;
class CWLPointerResource;
class IPointer;

class IKeyboardEventHandler {
  public:
    virtual ~IKeyboardEventHandler() = default;

    virtual void onKeyboardKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard> keyboard) = 0;
};

class CKeyboardEventHandlerStack {
  public:
    void push(WP<IKeyboardEventHandler> handler);
    bool remove(WP<IKeyboardEventHandler> handler);

    bool dispatch(const IKeyboard::SKeyEvent& event, SP<IKeyboard> keyboard, bool allowNewPress);
    void onKeyboardRemoved(SP<IKeyboard> keyboard);

  private:
    struct SOwnedKey {
        WP<IKeyboard>             keyboard;
        uint32_t                  keycode = 0;
        WP<IKeyboardEventHandler> handler;
    };

    void                                   pruneHandlers();

    std::vector<WP<IKeyboardEventHandler>> m_handlers;
    std::vector<SOwnedKey>                 m_ownedKeys;
};

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
    bool accepts(SP<CWLSurfaceResource> surf);
    void add(SP<CWLSurfaceResource> surf);
    void remove(SP<CWLSurfaceResource> surf);
    void setCallback(std::function<void()> onEnd_);
    void clear();

    bool m_keyboard = false;
    bool m_pointer  = false;

  private:
    std::vector<WP<CWLSurfaceResource>> m_surfs;
    std::function<void()>               m_onEnd;
    friend class CSeatManager;
};

class CSeatManager {
  public:
    // The physical key (as an evdev keycode) or mouse button held for the bind a pass comes from.
    // What it passes is kept held until either its release is passed, or this is released.
    struct SPassTrigger {
        uint32_t code  = 0;
        bool     mouse = false;

        bool     operator==(const SPassTrigger&) const = default;
    };

    CSeatManager();

    void                       updateCapabilities(uint32_t capabilities); // in IHID caps

    void                       setMouse(SP<IPointer> mouse);
    void                       setKeyboard(SP<IKeyboard> keeb);
    void                       updateActiveKeyboardData(); // updates the clients with the keymap and repeat info

    void                       setKeyboardFocus(SP<CWLSurfaceResource> surf);
    void                       sendKeyboardKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state);
    void                       sendKeyboardMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

    void                       holdPassedKey(uint32_t key, const SPassTrigger& trigger);
    void                       releasePassedKey(uint32_t key);
    void                       holdPassedButton(uint32_t button, SP<CWLSurfaceResource> surface, const SPassTrigger& trigger);
    void                       releasePassedButton(uint32_t button, SP<CWLSurfaceResource> surface);
    bool                       holdsPassedButtons() const;
    void                       releasePassHolds(const std::function<bool(const SPassTrigger&)>& released);

    CKeyboardEventHandlerStack m_keyboardEventHandlers;

    void                       setPointerFocus(SP<CWLSurfaceResource> surf, const Vector2D& local, bool releaseButtons = true);
    void                       sendPointerMotion(uint32_t timeMs, const Vector2D& local);
    void                       sendPointerButton(uint32_t timeMs, uint32_t key, wl_pointer_button_state state);
    void                       sendPointerFrame();
    void                       sendPointerFrame(WP<CWLSeatResource> pResource);
    void                       sendPointerAxis(uint32_t timeMs, wl_pointer_axis axis, double value, int32_t discrete, int32_t value120, wl_pointer_axis_source source,
                                               wl_pointer_axis_relative_direction relative);

    void                       sendTouchDown(SP<CWLSurfaceResource> surf, uint32_t timeMs, int32_t id, const Vector2D& local);
    void                       sendTouchUp(uint32_t timeMs, int32_t id);
    void                       sendTouchMotion(uint32_t timeMs, int32_t id, const Vector2D& local);
    void                       sendTouchFrame();
    void                       sendTouchCancel();
    void                       sendTouchShape(int32_t id, const Vector2D& shape);
    void                       sendTouchOrientation(int32_t id, double angle);

    void                       resendEnterEvents();

    uint32_t                   nextSerial(SP<CWLSeatResource> seatResource, bool enter = false);
    // pops the serial if it was valid, meaning it is consumed.
    bool                serialValid(SP<CWLSeatResource> seatResource, uint32_t serial, bool erase = true);
    void                recordPointerButtonSerial(SP<CWLSeatResource> seatResource, uint32_t serial, SP<CWLSurfaceResource> surface, uint32_t button);
    void                clearPointerButtonSerials(SP<CWLSeatResource> seatResource, SP<CWLSurfaceResource> surface, uint32_t button);
    bool                pointerButtonSerialValid(SP<CWLSeatResource> seatResource, uint32_t serial, SP<CWLSurfaceResource> surface, bool erase = true);

    void                onSetCursor(SP<CWLSeatResource> seatResource, uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& hotspot);

    SP<CWLSeatResource> seatResourceForClient(wl_client* client);

    struct {
        WP<CWLSurfaceResource> keyboardFocus;
        WP<CWLSeatResource>    keyboardFocusResource;

        WP<CWLSurfaceResource> pointerFocus;
        WP<CWLSeatResource>    pointerFocusResource;

        WP<CWLSurfaceResource> touchFocus;
        WP<CWLSeatResource>    touchFocusResource;

        WP<CWLSurfaceResource> dndPointerFocus;
    } m_state;

    struct SSetCursorEvent {
        SP<CWLSurfaceResource> surf = nullptr;
        Vector2D               hotspot;
    };

    struct {
        CSignalT<>                keyboardFocusChange;
        CSignalT<>                pointerFocusChange;
        CSignalT<>                dndPointerFocusChange;
        CSignalT<>                touchFocusChange;
        CSignalT<SSetCursorEvent> setCursor;
        CSignalT<>                setSelection;
        CSignalT<>                setPrimarySelection;
    } m_events;

    struct {
        WP<IDataSource>     currentSelection;
        CHyprSignalListener destroySelection;
        WP<IDataSource>     currentPrimarySelection;
        CHyprSignalListener destroyPrimarySelection;
    } m_selection;

    void setCurrentSelection(SP<IDataSource> source);
    void setCurrentPrimarySelection(SP<IDataSource> source);

    // do not write to directly, use set...
    WP<IPointer>  m_mouse;
    WP<IKeyboard> m_keyboard;

    void          setGrab(SP<CSeatGrab> grab); // nullptr removes
    SP<CSeatGrab> m_seatGrab;

  private:
    struct SPointerButtonSerial {
        uint32_t               serial = 0;
        uint32_t               button = 0;
        WP<CWLSurfaceResource> surface;
    };

    struct SSeatResourceContainer {
        SSeatResourceContainer(SP<CWLSeatResource>);

        WP<CWLSeatResource>               resource;
        uint32_t                          enterSerial = 0; // remember this forever
        std::vector<uint32_t>             serials;         // old -> new
        std::vector<SPointerButtonSerial> pointerButtonSerials;

        struct {
            CHyprSignalListener destroy;
        } listeners;
    };

    std::vector<SP<SSeatResourceContainer>> m_seatResources;
    void                                    onNewSeatResource(SP<CWLSeatResource> resource);
    SP<SSeatResourceContainer>              containerForResource(SP<CWLSeatResource> seatResource);

    void                                    refocusGrab();
    wl_client*                              passedKeysClient();
    void                                    leavePassedKeysClient(wl_client* client);

    struct SPassedKey {
        uint32_t     key = 0;
        SPassTrigger trigger;
    };

    struct SPassedButton {
        uint32_t                            button = 0;
        WP<CWLSurfaceResource>              surface;
        std::vector<WP<CWLPointerResource>> pointers; // the target's, which got the press
        SPassTrigger                        trigger;
    };

    // keys passed to Xwayland that are still held, see setKeyboardFocus()
    std::vector<SPassedKey> m_passedKeys;
    // buttons passed that are still held, they're kept pressed when the target loses pointer focus
    std::vector<SPassedButton> m_passedButtons;

    struct {
        CHyprSignalListener newSeatResource;
        CHyprSignalListener keyboardSurfaceDestroy;
        CHyprSignalListener pointerSurfaceDestroy;
        CHyprSignalListener touchSurfaceDestroy;
    } m_listeners;

    Vector2D m_lastLocalCoords;
    int      m_touchLocks = 0; // we assume there aint like 20 touch devices at once...

    friend struct SSeatResourceContainer;
    friend class CSeatGrab;
};

inline UP<CSeatManager> g_pSeatManager;
