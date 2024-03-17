#pragma once

#include "../../defines.hpp"
#include <list>
#include "../../helpers/WLClasses.hpp"
#include "../../Window.hpp"
#include "../../helpers/Timer.hpp"
#include "InputMethodRelay.hpp"

class CConstraint;

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
    CWindow*       touchFocusWindow  = nullptr;
    SLayerSurface* touchFocusLS      = nullptr;
    wlr_surface*   touchFocusSurface = nullptr;
    Vector2D       touchSurfaceOrigin;
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
    ~CInputManager();

    void               onMouseMoved(wlr_pointer_motion_event*);
    void               onMouseWarp(wlr_pointer_motion_absolute_event*);
    void               onMouseButton(wlr_pointer_button_event*);
    void               onMouseWheel(wlr_pointer_axis_event*);
    void               onKeyboardKey(wlr_keyboard_key_event*, SKeyboard*);
    void               onKeyboardMod(void*, SKeyboard*);

    void               newKeyboard(wlr_input_device*);
    void               newVirtualKeyboard(wlr_input_device*);
    void               newMouse(wlr_input_device*, bool virt = false);
    void               newTouchDevice(wlr_input_device*);
    void               newSwitch(wlr_input_device*);
    void               destroyTouchDevice(STouchDevice*);
    void               destroyKeyboard(SKeyboard*);
    void               destroyMouse(wlr_input_device*);
    void               destroySwitch(SSwitchDevice*);

    void               unconstrainMouse();
    bool               isConstrained();
    std::string        getActiveLayoutForKeyboard(SKeyboard*);

    Vector2D           getMouseCoordsInternal();
    void               refocus();
    void               simulateMouseMovement();
    void               sendMotionEventsToFocused();

    void               setKeyboardLayout();
    void               setPointerConfigs();
    void               setTouchDeviceConfigs(STouchDevice* dev = nullptr);
    void               setTabletConfigs();

    void               updateDragIcon();
    void               updateCapabilities();

    void               setClickMode(eClickBehaviorMode);
    eClickBehaviorMode getClickMode();
    void               processMouseRequest(wlr_seat_pointer_request_set_cursor_event* e);
    void               processMouseRequest(wlr_cursor_shape_manager_v1_request_set_shape_event* e);

    void               onTouchDown(wlr_touch_down_event*);
    void               onTouchUp(wlr_touch_up_event*);
    void               onTouchMove(wlr_touch_motion_event*);

    void               onPointerHoldBegin(wlr_pointer_hold_begin_event*);
    void               onPointerHoldEnd(wlr_pointer_hold_end_event*);

    STouchData         m_sTouchData;

    // for dragging floating windows
    CWindow*       currentlyDraggedWindow = nullptr;
    eMouseBindMode dragMode               = MBIND_INVALID;
    bool           m_bWasDraggingWindow   = false;

    // for refocus to be forced
    CWindow*             m_pForcedFocus = nullptr;

    SDrag                m_sDrag;

    std::list<SKeyboard> m_lKeyboards;
    std::list<SMouse>    m_lMice;

    // tablets
    std::list<STablet>     m_lTablets;
    std::list<STabletTool> m_lTabletTools;
    std::list<STabletPad>  m_lTabletPads;

    // idle inhibitors
    std::list<SIdleInhibitor> m_lIdleInhibitors;

    // Touch devices
    std::list<STouchDevice> m_lTouchDevices;

    // Switches
    std::list<SSwitchDevice> m_lSwitches;

    // Exclusive layer surfaces
    std::deque<SLayerSurface*> m_dExclusiveLSes;

    // constraints
    std::vector<CConstraint*> m_vConstraints;

    void                      newTabletTool(wlr_input_device*);
    void                      newTabletPad(wlr_input_device*);
    void                      focusTablet(STablet*, wlr_tablet_tool*, bool motion = false);
    void                      newIdleInhibitor(wlr_idle_inhibitor_v1*);
    void                      recheckIdleInhibitorStatus();

    void                      onSwipeBegin(wlr_pointer_swipe_begin_event*);
    void                      onSwipeEnd(wlr_pointer_swipe_end_event*);
    void                      onSwipeUpdate(wlr_pointer_swipe_update_event*);

    SSwipeGesture             m_sActiveSwipe;

    SKeyboard*                m_pActiveKeyboard = nullptr;

    CTimer                    m_tmrLastCursorMovement;

    CInputMethodRelay         m_sIMERelay;

    void                      updateKeyboardsLeds(wlr_input_device* pKeyboard);

    // for shared mods
    uint32_t accumulateModsFromAllKBs();

    // for virtual keyboards: whether we should respect them as normal ones
    bool shouldIgnoreVirtualKeyboard(SKeyboard*);

    // for special cursors that we choose
    void        setCursorImageUntilUnset(std::string);
    void        unsetCursorImage();

    std::string deviceNameToInternalString(std::string);
    std::string getNameForNewDevice(std::string);

    void        releaseAllMouseButtons();

    // for some bugs in follow mouse 0
    bool m_bLastFocusOnLS = false;

    // for hiding cursor on touch
    bool m_bLastInputTouch = false;

    // for tracking mouse refocus
    CWindow*     m_pLastMouseFocus   = nullptr;
    wlr_surface* m_pLastMouseSurface = nullptr;

    //
    bool m_bEmptyFocusCursorSet = false;

  private:
    bool                 m_bCursorImageOverridden = false;
    eBorderIconDirection m_eBorderIconDirection   = BORDERICON_NONE;

    // for click behavior override
    eClickBehaviorMode m_ecbClickBehavior      = CLICKMODE_DEFAULT;
    Vector2D           m_vLastCursorPosFloored = Vector2D();

    void               processMouseDownNormal(wlr_pointer_button_event* e);
    void               processMouseDownKill(wlr_pointer_button_event* e);

    bool               cursorImageUnlocked();

    void               disableAllKeyboards(bool virt = false);

    uint32_t           m_uiCapabilities = 0;

    void               mouseMoveUnified(uint32_t, bool refocus = false);

    STabletTool*       ensureTabletToolPresent(wlr_tablet_tool*);

    void               applyConfigToKeyboard(SKeyboard*);

    // this will be set after a refocus()
    wlr_surface*   m_pFoundSurfaceToFocus = nullptr;
    SLayerSurface* m_pFoundLSToFocus      = nullptr;
    CWindow*       m_pFoundWindowToFocus  = nullptr;

    // for holding focus on buttons held
    bool m_bFocusHeldByButtons   = false;
    bool m_bRefocusHeldByButtons = false;

    // for releasing mouse buttons
    std::list<uint32_t> m_lCurrentlyHeldButtons;

    // swipe
    void beginWorkspaceSwipe();
    void updateWorkspaceSwipe(double);
    void endWorkspaceSwipe();

    void setBorderCursorIcon(eBorderIconDirection);
    void setCursorIconOnBorder(CWindow* w);

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

    bool m_bExitTriggered = false; // for exit dispatcher

    friend class CKeybindManager;
    friend class CWLSurface;
};

inline std::unique_ptr<CInputManager> g_pInputManager;
