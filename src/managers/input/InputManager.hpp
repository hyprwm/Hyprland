#pragma once

#include "../../defines.hpp"
#include <list>
#include <any>
#include "../../helpers/WLClasses.hpp"
#include "../../helpers/Timer.hpp"
#include "InputMethodRelay.hpp"
#include "../../helpers/signal/Listener.hpp"
#include "../../devices/IPointer.hpp"
#include "../../devices/ITouch.hpp"
#include "../../devices/Tablet.hpp"

class CPointerConstraint;
class CWindow;
class CIdleInhibitor;
class CVirtualKeyboardV1Resource;
class CVirtualPointerV1Resource;
class IKeyboard;

enum eClickBehaviorMode {
    CLICKMODE_DEFAULT = 0,
    CLICKMODE_KILL
};

enum eMouseBindMode {
    MBIND_INVALID            = -1,
    MBIND_MOVE               = 0,
    MBIND_RESIZE             = 1,
    MBIND_RESIZE_BLOCK_RATIO = 2,
    MBIND_RESIZE_FORCE_RATIO = 3
};

enum eBorderIconDirection {
    BORDERICON_NONE,
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
    PHLWINDOWREF           touchFocusWindow;
    PHLLSREF               touchFocusLS;
    WP<CWLSurfaceResource> touchFocusSurface;
    Vector2D               touchSurfaceOrigin;
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

    void               newKeyboard(wlr_input_device*);
    void               newVirtualKeyboard(SP<CVirtualKeyboardV1Resource>);
    void               newMouse(wlr_input_device*);
    void               newVirtualMouse(SP<CVirtualPointerV1Resource>);
    void               newTouchDevice(wlr_input_device*);
    void               newSwitch(wlr_input_device*);
    void               newTabletTool(wlr_tablet_tool*);
    void               newTabletPad(wlr_input_device*);
    void               newTablet(wlr_input_device*);
    void               destroyTouchDevice(SP<ITouch>);
    void               destroyKeyboard(SP<IKeyboard>);
    void               destroyPointer(SP<IPointer>);
    void               destroyTablet(SP<CTablet>);
    void               destroyTabletTool(SP<CTabletTool>);
    void               destroyTabletPad(SP<CTabletPad>);
    void               destroySwitch(SSwitchDevice*);

    void               unconstrainMouse();
    bool               isConstrained();

    Vector2D           getMouseCoordsInternal();
    void               refocus();
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

    STouchData         m_sTouchData;

    // for dragging floating windows
    PHLWINDOWREF   currentlyDraggedWindow;
    eMouseBindMode dragMode             = MBIND_INVALID;
    bool           m_bWasDraggingWindow = false;

    // for refocus to be forced
    PHLWINDOWREF                 m_pForcedFocus;

    std::vector<SP<IKeyboard>>   m_vKeyboards;
    std::vector<SP<IPointer>>    m_vPointers;
    std::vector<SP<ITouch>>      m_vTouches;
    std::vector<SP<CTablet>>     m_vTablets;
    std::vector<SP<CTabletTool>> m_vTabletTools;
    std::vector<SP<CTabletPad>>  m_vTabletPads;
    std::vector<WP<IHID>>        m_vHIDs; // general container for all HID devices connected to the input manager.

    // Switches
    std::list<SSwitchDevice> m_lSwitches;

    // Exclusive layer surfaces
    std::deque<PHLLSREF> m_dExclusiveLSes;

    // constraints
    std::vector<WP<CPointerConstraint>> m_vConstraints;

    //
    void              newIdleInhibitor(std::any);
    void              recheckIdleInhibitorStatus();

    SSwipeGesture     m_sActiveSwipe;

    CTimer            m_tmrLastCursorMovement;

    CInputMethodRelay m_sIMERelay;

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
    bool m_bLastFocusOnLS       = false;
    bool m_bLastFocusOnIMEPopup = false;

    // for hard input e.g. clicks
    bool m_bHardInput = false;

    // for hiding cursor on touch
    bool m_bLastInputTouch = false;

    // for tracking mouse refocus
    PHLWINDOWREF m_pLastMouseFocus;

    //
    bool m_bEmptyFocusCursorSet = false;

  private:
    // Listeners
    struct {
        CHyprSignalListener setCursorShape;
        CHyprSignalListener newIdleInhibitor;
        CHyprSignalListener newVirtualKeyboard;
        CHyprSignalListener newVirtualMouse;
        CHyprSignalListener setCursor;
    } m_sListeners;

    bool                 m_bCursorImageOverridden = false;
    eBorderIconDirection m_eBorderIconDirection   = BORDERICON_NONE;

    // for click behavior override
    eClickBehaviorMode m_ecbClickBehavior      = CLICKMODE_DEFAULT;
    Vector2D           m_vLastCursorPosFloored = Vector2D();

    void               setupKeyboard(SP<IKeyboard> keeb);
    void               setupMouse(SP<IPointer> mauz);

    void               processMouseDownNormal(const IPointer::SButtonEvent& e);
    void               processMouseDownKill(const IPointer::SButtonEvent& e);

    bool               cursorImageUnlocked();

    void               disableAllKeyboards(bool virt = false);

    uint32_t           m_uiCapabilities = 0;

    void               mouseMoveUnified(uint32_t, bool refocus = false);

    SP<CTabletTool>    ensureTabletToolPresent(wlr_tablet_tool*);

    void               applyConfigToKeyboard(SP<IKeyboard>);

    // this will be set after a refocus()
    WP<CWLSurfaceResource> m_pFoundSurfaceToFocus;
    PHLLSREF               m_pFoundLSToFocus;
    PHLWINDOWREF           m_pFoundWindowToFocus;

    // for holding focus on buttons held
    bool m_bFocusHeldByButtons   = false;
    bool m_bRefocusHeldByButtons = false;

    // for releasing mouse buttons
    std::list<uint32_t> m_lCurrentlyHeldButtons;

    // idle inhibitors
    struct SIdleInhibitor {
        SP<CIdleInhibitor>  inhibitor;
        bool                nonDesktop = false;
        CHyprSignalListener surfaceDestroyListener;
    };
    std::vector<std::unique_ptr<SIdleInhibitor>> m_vIdleInhibitors;

    // swipe
    void beginWorkspaceSwipe();
    void updateWorkspaceSwipe(double);
    void endWorkspaceSwipe();

    void setBorderCursorIcon(eBorderIconDirection);
    void setCursorIconOnBorder(PHLWINDOW w);

    // temporary. Obeys setUntilUnset.
    void setCursorImageOverride(const std::string& name);

    // cursor surface
    struct cursorSI {
        bool           hidden = false; // null surface = hidden
        SP<CWLSurface> wlSurface;
        Vector2D       vHotspot;
        std::string    name; // if not empty, means set by name.
        bool           inUse = false;
    } m_sCursorSurfaceInfo;

    void restoreCursorIconToApp(); // no-op if restored

    friend class CKeybindManager;
    friend class CWLSurface;
};

inline std::unique_ptr<CInputManager> g_pInputManager;
