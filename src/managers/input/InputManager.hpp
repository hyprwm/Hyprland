#pragma once

#include "../../defines.hpp"
#include <list>
#include <any>
#include "../../helpers/WLClasses.hpp"
#include "../../helpers/time/Timer.hpp"
#include "InputMethodRelay.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../devices/IPointer.hpp"
#include "../../devices/ITouch.hpp"
#include "../../devices/Tablet.hpp"
#include "../SessionLockManager.hpp"

class CPointerConstraint;
class CWindow;
class CIdleInhibitor;
class CVirtualKeyboardV1Resource;
class CVirtualPointerV1Resource;
class IKeyboard;

AQUAMARINE_FORWARD(IPointer);
AQUAMARINE_FORWARD(IKeyboard);
AQUAMARINE_FORWARD(ITouch);
AQUAMARINE_FORWARD(ISwitch);
AQUAMARINE_FORWARD(ITablet);
AQUAMARINE_FORWARD(ITabletTool);
AQUAMARINE_FORWARD(ITabletPad);

enum eClickBehaviorMode : uint8_t {
    CLICKMODE_DEFAULT = 0,
    CLICKMODE_KILL
};

enum eMouseBindMode : int8_t {
    MBIND_INVALID            = -1,
    MBIND_MOVE               = 0,
    MBIND_RESIZE             = 1,
    MBIND_RESIZE_BLOCK_RATIO = 2,
    MBIND_RESIZE_FORCE_RATIO = 3
};

enum eBorderIconDirection : uint8_t {
    BORDERICON_NONE = 0,
    BORDERICON_UP,
    BORDERICON_DOWN,
    BORDERICON_LEFT,
    BORDERICON_RIGHT,
    BORDERICON_UP_LEFT,
    BORDERICON_DOWN_LEFT,
    BORDERICON_UP_RIGHT,
    BORDERICON_DOWN_RIGHT,
};

struct STouchData {
    WP<SSessionLockSurface> touchFocusLockSurface;
    PHLWINDOWREF            touchFocusWindow;
    PHLLSREF                touchFocusLS;
    WP<CWLSurfaceResource>  touchFocusSurface;
    Vector2D                touchSurfaceOrigin;
};

// The third row is always 0 0 1 and is not expected by `libinput_device_config_calibration_set_matrix`
static const float MATRICES[8][6] = {{// normal
                                      1, 0, 0, 0, 1, 0},
                                     {// rotation 90°
                                      0, -1, 1, 1, 0, 0},
                                     {// rotation 180°
                                      -1, 0, 1, 0, -1, 1},
                                     {// rotation 270°
                                      0, 1, 0, -1, 0, 1},
                                     {// flipped
                                      -1, 0, 1, 0, 1, 0},
                                     {// flipped + rotation 90°
                                      0, 1, 0, 1, 0, 0},
                                     {// flipped + rotation 180°
                                      1, 0, 0, 0, -1, 1},
                                     {// flipped + rotation 270°
                                      0, -1, 1, -1, 0, 1}};

class CKeybindManager;

class CInputManager {
  public:
    CInputManager();
    ~CInputManager();

    void               onMouseMoved(IPointer::SMotionEvent);
    void               onMouseWarp(IPointer::SMotionAbsoluteEvent);
    void               onMouseButton(IPointer::SButtonEvent);
    void               onMouseWheel(IPointer::SAxisEvent);
    void               onKeyboardKey(std::any, SP<IKeyboard>);
    void               onKeyboardMod(SP<IKeyboard>);

    void               newKeyboard(SP<Aquamarine::IKeyboard>);
    void               newVirtualKeyboard(SP<CVirtualKeyboardV1Resource>);
    void               newMouse(SP<Aquamarine::IPointer>);
    void               newVirtualMouse(SP<CVirtualPointerV1Resource>);
    void               newTouchDevice(SP<Aquamarine::ITouch>);
    void               newSwitch(SP<Aquamarine::ISwitch>);
    void               newTabletPad(SP<Aquamarine::ITabletPad>);
    void               newTablet(SP<Aquamarine::ITablet>);
    void               destroyTouchDevice(SP<ITouch>);
    void               destroyKeyboard(SP<IKeyboard>);
    void               destroyPointer(SP<IPointer>);
    void               destroyTablet(SP<CTablet>);
    void               destroyTabletTool(SP<CTabletTool>);
    void               destroyTabletPad(SP<CTabletPad>);
    void               destroySwitch(SSwitchDevice*);

    void               unconstrainMouse();
    bool               isConstrained();
    bool               isLocked();

    Vector2D           getMouseCoordsInternal();
    void               refocus();
    bool               refocusLastWindow(PHLMONITOR pMonitor);
    void               simulateMouseMovement();
    void               sendMotionEventsToFocused();

    void               setKeyboardLayout();
    void               setPointerConfigs();
    void               setTouchDeviceConfigs(SP<ITouch> dev = nullptr);
    void               setTabletConfigs();

    void               updateCapabilities();
    void               updateKeyboardsLeds(SP<IKeyboard>);

    void               setClickMode(eClickBehaviorMode);
    eClickBehaviorMode getClickMode();
    void               processMouseRequest(std::any e);

    void               onTouchDown(ITouch::SDownEvent);
    void               onTouchUp(ITouch::SUpEvent);
    void               onTouchMove(ITouch::SMotionEvent);

    void               onSwipeBegin(IPointer::SSwipeBeginEvent);
    void               onSwipeEnd(IPointer::SSwipeEndEvent);
    void               onSwipeUpdate(IPointer::SSwipeUpdateEvent);

    void               onTabletAxis(CTablet::SAxisEvent);
    void               onTabletProximity(CTablet::SProximityEvent);
    void               onTabletTip(CTablet::STipEvent);
    void               onTabletButton(CTablet::SButtonEvent);

    STouchData         m_touchData;

    // for dragging floating windows
    PHLWINDOWREF   m_currentlyDraggedWindow;
    eMouseBindMode m_dragMode             = MBIND_INVALID;
    bool           m_wasDraggingWindow    = false;
    bool           m_dragThresholdReached = false;

    // for refocus to be forced
    PHLWINDOWREF                 m_forcedFocus;

    std::vector<SP<IKeyboard>>   m_keyboards;
    std::vector<SP<IPointer>>    m_pointers;
    std::vector<SP<ITouch>>      m_touches;
    std::vector<SP<CTablet>>     m_tablets;
    std::vector<SP<CTabletTool>> m_tabletTools;
    std::vector<SP<CTabletPad>>  m_tabletPads;
    std::vector<WP<IHID>>        m_hids; // general container for all HID devices connected to the input manager.

    // Switches
    std::list<SSwitchDevice> m_switches;

    // Exclusive layer surfaces
    std::vector<PHLLSREF> m_exclusiveLSes;

    // constraints
    std::vector<WP<CPointerConstraint>> m_constraints;

    //
    void              newIdleInhibitor(std::any);
    void              recheckIdleInhibitorStatus();
    bool              isWindowInhibiting(const PHLWINDOW& pWindow, bool onlyHl = true);

    SSwipeGesture     m_activeSwipe;

    CTimer            m_lastCursorMovement;

    CInputMethodRelay m_relay;

    // for shared mods
    uint32_t accumulateModsFromAllKBs();

    // for virtual keyboards: whether we should respect them as normal ones
    bool shouldIgnoreVirtualKeyboard(SP<IKeyboard>);

    // for special cursors that we choose
    void        setCursorImageUntilUnset(std::string);
    void        unsetCursorImage();

    std::string deviceNameToInternalString(std::string);
    std::string getNameForNewDevice(std::string);

    void        releaseAllMouseButtons();

    // for some bugs in follow mouse 0
    bool m_lastFocusOnLS = false;

    // for hard input e.g. clicks
    bool m_hardInput = false;

    // for hiding cursor on touch
    bool m_lastInputTouch = false;

    // for tracking mouse refocus
    PHLWINDOWREF m_lastMouseFocus;

    //
    bool m_emptyFocusCursorSet = false;

  private:
    // Listeners
    struct {
        CHyprSignalListener setCursorShape;
        CHyprSignalListener newIdleInhibitor;
        CHyprSignalListener newVirtualKeyboard;
        CHyprSignalListener newVirtualMouse;
        CHyprSignalListener setCursor;
    } m_listeners;

    bool                 m_cursorImageOverridden = false;
    eBorderIconDirection m_borderIconDirection   = BORDERICON_NONE;

    // for click behavior override
    eClickBehaviorMode m_clickBehavior        = CLICKMODE_DEFAULT;
    Vector2D           m_lastCursorPosFloored = Vector2D();

    void               setupKeyboard(SP<IKeyboard> keeb);
    void               setupMouse(SP<IPointer> mauz);

    void               processMouseDownNormal(const IPointer::SButtonEvent& e);
    void               processMouseDownKill(const IPointer::SButtonEvent& e);

    bool               cursorImageUnlocked();

    void               disableAllKeyboards(bool virt = false);

    uint32_t           m_capabilities = 0;

    void               mouseMoveUnified(uint32_t, bool refocus = false, bool mouse = false);
    void               recheckMouseWarpOnMouseInput();

    SP<CTabletTool>    ensureTabletToolPresent(SP<Aquamarine::ITabletTool>);

    void               applyConfigToKeyboard(SP<IKeyboard>);

    // this will be set after a refocus()
    WP<CWLSurfaceResource> m_foundSurfaceToFocus;
    PHLLSREF               m_foundLSToFocus;
    PHLWINDOWREF           m_foundWindowToFocus;

    // used for warping back after non-mouse input
    Vector2D m_lastMousePos   = {};
    double   m_mousePosDelta  = 0;
    bool     m_lastInputMouse = true;

    // for holding focus on buttons held
    bool m_focusHeldByButtons   = false;
    bool m_refocusHeldByButtons = false;

    // for releasing mouse buttons
    std::list<uint32_t> m_currentlyHeldButtons;

    // idle inhibitors
    struct SIdleInhibitor {
        SP<CIdleInhibitor>  inhibitor;
        bool                nonDesktop = false;
        CHyprSignalListener surfaceDestroyListener;
    };
    std::vector<UP<SIdleInhibitor>> m_idleInhibitors;

    // swipe
    void beginWorkspaceSwipe();
    void updateWorkspaceSwipe(double);
    void endWorkspaceSwipe();

    void setBorderCursorIcon(eBorderIconDirection);
    void setCursorIconOnBorder(PHLWINDOW w);

    // temporary. Obeys setUntilUnset.
    void setCursorImageOverride(const std::string& name);

    // cursor surface
    struct {
        bool           hidden = false; // null surface = hidden
        SP<CWLSurface> wlSurface;
        Vector2D       vHotspot;
        std::string    name; // if not empty, means set by name.
        bool           inUse = false;
    } m_cursorSurfaceInfo;

    void restoreCursorIconToApp(); // no-op if restored

    // discrete scrolling emulation using v120 data
    struct {
        bool     lastEventSign     = false;
        bool     lastEventAxis     = false;
        uint32_t lastEventTime     = 0;
        uint32_t accumulatedScroll = 0;
    } m_scrollWheelState;

    friend class CKeybindManager;
    friend class CWLSurface;
};

inline UP<CInputManager> g_pInputManager;
