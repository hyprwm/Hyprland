#pragma once

#include "../../defines.hpp"
#include <list>
#include "../../helpers/WLClasses.hpp"
#include "../../Window.hpp"
#include "../../helpers/Timer.hpp"
#include "InputMethodRelay.hpp"

enum eClickBehaviorMode {
    CLICKMODE_DEFAULT = 0,
    CLICKMODE_KILL
};

struct STouchData {
    CWindow* touchFocusWindow = nullptr;
};

class CInputManager {
public:

    void            onMouseMoved(wlr_pointer_motion_event*);
    void            onMouseWarp(wlr_pointer_motion_absolute_event*);
    void            onMouseButton(wlr_pointer_button_event*);
    void            onMouseWheel(wlr_pointer_axis_event*); 
    void            onKeyboardKey(wlr_keyboard_key_event*, SKeyboard*);
    void            onKeyboardMod(void*, SKeyboard*);

    void            newKeyboard(wlr_input_device*);
    void            newVirtualKeyboard(wlr_input_device*);
    void            newMouse(wlr_input_device*, bool virt = false);
    void            destroyKeyboard(SKeyboard*);
    void            destroyMouse(wlr_input_device*);

    void            constrainMouse(SMouse*, wlr_pointer_constraint_v1*);
    void            recheckConstraint(SMouse*);

    Vector2D        getMouseCoordsInternal();
    void            refocus();

    void            setKeyboardLayout();
    void            setMouseConfigs();

    void            updateDragIcon();
    void            updateCapabilities(wlr_input_device*);

    void            setClickMode(eClickBehaviorMode);
    eClickBehaviorMode getClickMode();
    void            processMouseRequest(wlr_seat_pointer_request_set_cursor_event*);

    void            onTouchDown(wlr_touch_down_event*);
    void            onTouchUp(wlr_touch_up_event*);
    void            onTouchMove(wlr_touch_motion_event*);

    STouchData      m_sTouchData;

    // for dragging floating windows
    CWindow*        currentlyDraggedWindow = nullptr;
    int             dragButton = -1;

    SDrag           m_sDrag;

    std::list<SConstraint>  m_lConstraints;
    std::list<SKeyboard>    m_lKeyboards;
    std::list<SMouse>       m_lMice;

    // tablets
    std::list<STablet>      m_lTablets;
    std::list<STabletTool>  m_lTabletTools;
    std::list<STabletPad>   m_lTabletPads;

    // idle inhibitors
    std::list<SIdleInhibitor> m_lIdleInhibitors;

    void            newTabletTool(wlr_input_device*);
    void            newTabletPad(wlr_input_device*);
    void            focusTablet(STablet*, wlr_tablet_tool*, bool motion = false);
    void            newIdleInhibitor(wlr_idle_inhibitor_v1*);
    void            recheckIdleInhibitorStatus();

    void            onSwipeBegin(wlr_pointer_swipe_begin_event*);
    void            onSwipeEnd(wlr_pointer_swipe_end_event*);
    void            onSwipeUpdate(wlr_pointer_swipe_update_event*);

    SSwipeGesture   m_sActiveSwipe;

    SKeyboard*      m_pActiveKeyboard = nullptr;

    CTimer          m_tmrLastCursorMovement;

    CInputMethodRelay m_sIMERelay;

    // for shared mods
    uint32_t        accumulateModsFromAllKBs();

    CWindow*        m_pFollowOnDnDBegin = nullptr;

private:

    // for click behavior override
    eClickBehaviorMode m_ecbClickBehavior = CLICKMODE_DEFAULT;
    Vector2D        m_vLastCursorPosFloored = Vector2D();

    void            processMouseDownNormal(wlr_pointer_button_event* e);
    void            processMouseDownKill(wlr_pointer_button_event* e);

    uint32_t        m_uiCapabilities = 0;

    void            mouseMoveUnified(uint32_t, bool refocus = false);

    STabletTool*    ensureTabletToolPresent(wlr_tablet_tool*);

    void            applyConfigToKeyboard(SKeyboard*);
};

inline std::unique_ptr<CInputManager> g_pInputManager;