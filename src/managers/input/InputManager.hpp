#pragma once

#include "../../defines.hpp"
#include <list>
#include <any>
#include "../../helpers/WLClasses.hpp"
#include "../../helpers/Timer.hpp"
#include "InputMethodRelay.hpp"
#include "../../helpers/signal/Listener.hpp"

class CPointerConstraint;
class CWindow;
class CIdleInhibitor;
class CVirtualKeyboardV1Resource;
class CVirtualPointerV1Resource;
class IPointer;
class IKeyboard;
class ITouch;

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
    PHLWINDOWREF touchFocusWindow;
    PHLLSREF     touchFocusLS;
    wlr_surface* touchFocusSurface = nullptr;
    Vector2D     touchSurfaceOrigin;
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

    void               onMouseMoved(wlr_pointer_motion_event*);
    void               onMouseWarp(wlr_pointer_motion_absolute_event*);
    void               onMouseButton(wlr_pointer_button_event*);
    void               onMouseWheel(wlr_pointer_axis_event*);
    void               onKeyboardKey(std::any, SP<IKeyboard>);
    void               onKeyboardMod(SP<IKeyboard>);

    void               newKeyboard(wlr_input_device*);
    void               newVirtualKeyboard(SP<CVirtualKeyboardV1Resource>);
    void               newMouse(wlr_input_device*);
    void               newVirtualMouse(SP<CVirtualPointerV1Resource>);
    void               newTouchDevice(wlr_input_device*);
    void               newSwitch(wlr_input_device*);
    void               destroyTouchDevice(SP<ITouch>);
    void               destroyKeyboard(SP<IKeyboard>);
    void               destroyPointer(SP<IPointer>);
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

    void               updateDragIcon();
    void               updateCapabilities();

    void               setClickMode(eClickBehaviorMode);
    eClickBehaviorMode getClickMode();
    void               processMouseRequest(wlr_seat_pointer_request_set_cursor_event* e);

    void               onTouchDown(wlr_touch_down_event*);
    void               onTouchUp(wlr_touch_up_event*);
    void               onTouchMove(wlr_touch_motion_event*);

    STouchData         m_sTouchData;

    // for dragging floating windows
    PHLWINDOWREF   currentlyDraggedWindow;
    eMouseBindMode dragMode             = MBIND_INVALID;
    bool           m_bWasDraggingWindow = false;

    // for refocus to be forced
    PHLWINDOWREF               m_pForcedFocus;

    SDrag                      m_sDrag;

    std::vector<SP<IKeyboard>> m_vKeyboards;
    std::vector<SP<IPointer>>  m_vPointers;
    std::vector<SP<ITouch>>    m_vTouches;

    // tablets
    std::list<STablet>     m_lTablets;
    std::list<STabletTool> m_lTabletTools;
    std::list<STabletPad>  m_lTabletPads;

    // Switches
    std::list<SSwitchDevice> m_lSwitches;

    // Exclusive layer surfaces
    std::deque<PHLLSREF> m_dExclusiveLSes;

    // constraints
    std::vector<std::weak_ptr<CPointerConstraint>> m_vConstraints;

    //
    void              newTabletTool(wlr_input_device*);
    void              newTabletPad(wlr_input_device*);
    void              focusTablet(STablet*, wlr_tablet_tool*, bool motion = false);
    void              newIdleInhibitor(std::any);
    void              recheckIdleInhibitorStatus();

    void              onSwipeBegin(wlr_pointer_swipe_begin_event*);
    void              onSwipeEnd(wlr_pointer_swipe_end_event*);
    void              onSwipeUpdate(wlr_pointer_swipe_update_event*);

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
    bool m_bLastFocusOnLS = false;

    bool m_bLastFocusOnIMEPopup = false;

    // for hiding cursor on touch
    bool m_bLastInputTouch = false;

    // for tracking mouse refocus
    PHLWINDOWREF m_pLastMouseFocus;
    wlr_surface* m_pLastMouseSurface = nullptr;

    //
    bool m_bEmptyFocusCursorSet = false;

  private:
    // Listeners
    struct {
        CHyprSignalListener setCursorShape;
        CHyprSignalListener newIdleInhibitor;
        CHyprSignalListener newVirtualKeyboard;
        CHyprSignalListener newVirtualMouse;
    } m_sListeners;

    bool                 m_bCursorImageOverridden = false;
    eBorderIconDirection m_eBorderIconDirection   = BORDERICON_NONE;

    // for click behavior override
    eClickBehaviorMode m_ecbClickBehavior      = CLICKMODE_DEFAULT;
    Vector2D           m_vLastCursorPosFloored = Vector2D();

    void               setupKeyboard(SP<IKeyboard> keeb);
    void               setupMouse(SP<IPointer> mauz);

    void               processMouseDownNormal(wlr_pointer_button_event* e);
    void               processMouseDownKill(wlr_pointer_button_event* e);

    bool               cursorImageUnlocked();

    void               disableAllKeyboards(bool virt = false);

    uint32_t           m_uiCapabilities = 0;

    void               mouseMoveUnified(uint32_t, bool refocus = false);

    STabletTool*       ensureTabletToolPresent(wlr_tablet_tool*);

    void               applyConfigToKeyboard(SP<IKeyboard>);

    // this will be set after a refocus()
    wlr_surface* m_pFoundSurfaceToFocus = nullptr;
    PHLLSREF     m_pFoundLSToFocus;
    PHLWINDOWREF m_pFoundWindowToFocus;

    // for holding focus on buttons held
    bool m_bFocusHeldByButtons   = false;
    bool m_bRefocusHeldByButtons = false;

    // for releasing mouse buttons
    std::list<uint32_t> m_lCurrentlyHeldButtons;

    // idle inhibitors
    struct SIdleInhibitor {
        std::shared_ptr<CIdleInhibitor> inhibitor;
        PHLWINDOWREF                    pWindow;
        CHyprSignalListener             windowDestroyListener;
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
        bool        hidden = false; // null surface = hidden
        CWLSurface  wlSurface;
        Vector2D    vHotspot;
        std::string name; // if not empty, means set by name.
        bool        inUse = false;
    } m_sCursorSurfaceInfo;

    void restoreCursorIconToApp(); // no-op if restored

    friend class CKeybindManager;
    friend class CWLSurface;
};

inline std::unique_ptr<CInputManager> g_pInputManager;
