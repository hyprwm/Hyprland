#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/IdleNotify.hpp"
#include "../../protocols/Tablet.hpp"
#include "../../devices/Tablet.hpp"
#include "../../managers/PointerManager.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../protocols/PointerConstraints.hpp"

static void unfocusTool(SP<CTabletTool> tool) {
    if (!tool->getSurface())
        return;

    tool->setSurface(nullptr);
    if (tool->isDown)
        PROTO::tablet->up(tool);
    for (auto& b : tool->buttonsDown) {
        PROTO::tablet->buttonTool(tool, b, false);
    }
    PROTO::tablet->proximityOut(tool);
}

static void focusTool(SP<CTabletTool> tool, SP<CTablet> tablet, SP<CWLSurfaceResource> surf) {
    if (tool->getSurface() == surf || !surf)
        return;

    if (tool->getSurface() && tool->getSurface() != surf)
        unfocusTool(tool);

    tool->setSurface(surf);
    PROTO::tablet->proximityIn(tool, tablet, surf);
    if (tool->isDown)
        PROTO::tablet->down(tool);
    for (auto& b : tool->buttonsDown) {
        PROTO::tablet->buttonTool(tool, b, true);
    }
}

static void refocusTablet(SP<CTablet> tab, SP<CTabletTool> tool, bool motion = false) {
    const auto LASTHLSURFACE = CWLSurface::fromResource(g_pSeatManager->state.pointerFocus.lock());

    if (!LASTHLSURFACE || !tool->active) {
        if (tool->getSurface())
            unfocusTool(tool);

        return;
    }

    const auto BOX = LASTHLSURFACE->getSurfaceBoxGlobal();

    if (!BOX.has_value()) {
        if (tool->getSurface())
            unfocusTool(tool);

        return;
    }

    const auto CURSORPOS = g_pInputManager->getMouseCoordsInternal();

    focusTool(tool, tab, g_pSeatManager->state.pointerFocus.lock());

    if (!motion)
        return;

    if (LASTHLSURFACE->constraint() && tool->wlr()->type != WLR_TABLET_TOOL_TYPE_MOUSE) {
        // cursor logic will completely break here as the cursor will be locked.
        // let's just "map" the desired position to the constraint area.

        Vector2D local;

        // yes, this technically ignores any regions set by the app. Too bad!
        if (LASTHLSURFACE->getWindow())
            local = tool->absolutePos * LASTHLSURFACE->getWindow()->m_vRealSize.goal();
        else
            local = tool->absolutePos * BOX->size();

        if (LASTHLSURFACE->getWindow() && LASTHLSURFACE->getWindow()->m_bIsX11)
            local = local * LASTHLSURFACE->getWindow()->m_fX11SurfaceScaledBy;

        PROTO::tablet->motion(tool, local);
        return;
    }

    auto local = CURSORPOS - BOX->pos();

    if (LASTHLSURFACE->getWindow() && LASTHLSURFACE->getWindow()->m_bIsX11)
        local = local * LASTHLSURFACE->getWindow()->m_fX11SurfaceScaledBy;

    PROTO::tablet->motion(tool, local);
}

void CInputManager::onTabletAxis(CTablet::SAxisEvent e) {
    const auto PTAB  = e.tablet;
    const auto PTOOL = ensureTabletToolPresent(e.tool);

    if (PTOOL->active && (e.updatedAxes & (CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_X | CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_Y))) {
        double   x  = (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_X) ? e.axis.x : NAN;
        double   dx = (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_X) ? e.axisDelta.x : NAN;
        double   y  = (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_Y) ? e.axis.y : NAN;
        double   dy = (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_Y) ? e.axisDelta.y : NAN;

        Vector2D delta = {std::isnan(dx) ? 0.0 : dx, std::isnan(dy) ? 0.0 : dy};

        switch (e.tool->type) {
            case WLR_TABLET_TOOL_TYPE_MOUSE: {
                g_pPointerManager->move(delta);
                break;
            }
            default: {
                if (!std::isnan(x))
                    PTOOL->absolutePos.x = x;
                if (!std::isnan(y))
                    PTOOL->absolutePos.y = y;

                if (PTAB->relativeInput)
                    g_pPointerManager->move(delta);
                else {
                    //Calculate transformations if active area is set
                    if (!PTAB->activeArea.empty()) {
                        if (!std::isnan(x))
                            x = (x - PTAB->activeArea.x) / (PTAB->activeArea.w - PTAB->activeArea.x);
                        if (!std::isnan(y))
                            y = (y - PTAB->activeArea.y) / (PTAB->activeArea.h - PTAB->activeArea.y);
                    }
                    g_pPointerManager->warpAbsolute({x, y}, PTAB);
                }
                break;
            }
        }

        refocusTablet(PTAB, PTOOL, true);
        m_tmrLastCursorMovement.reset();
    }

    if (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_PRESSURE)
        PROTO::tablet->pressure(PTOOL, e.pressure);

    if (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_DISTANCE)
        PROTO::tablet->distance(PTOOL, e.distance);

    if (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_ROTATION)
        PROTO::tablet->rotation(PTOOL, e.rotation);

    if (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_SLIDER)
        PROTO::tablet->slider(PTOOL, e.slider);

    if (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_WHEEL)
        PROTO::tablet->wheel(PTOOL, e.wheelDelta);

    if (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_TILT_X)
        PTOOL->tilt.x = e.tilt.x;

    if (e.updatedAxes & CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_TILT_Y)
        PTOOL->tilt.y = e.tilt.y;

    if (e.updatedAxes & (CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_TILT_X | CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_TILT_Y))
        PROTO::tablet->tilt(PTOOL, PTOOL->tilt);

    PROTO::idle->onActivity();
}

void CInputManager::onTabletTip(CTablet::STipEvent e) {
    const auto PTAB  = e.tablet;
    const auto PTOOL = ensureTabletToolPresent(e.tool);

    if (e.in) {
        simulateMouseMovement();
        refocusTablet(PTAB, PTOOL);
        PROTO::tablet->down(PTOOL);
    } else
        PROTO::tablet->up(PTOOL);

    PTOOL->isDown = e.in;

    PROTO::idle->onActivity();
}

void CInputManager::onTabletButton(CTablet::SButtonEvent e) {
    const auto PTOOL = ensureTabletToolPresent(e.tool);

    PROTO::tablet->buttonTool(PTOOL, e.button, e.down);

    if (e.down)
        PTOOL->buttonsDown.push_back(e.button);
    else
        std::erase(PTOOL->buttonsDown, e.button);

    PROTO::idle->onActivity();
}

void CInputManager::onTabletProximity(CTablet::SProximityEvent e) {
    const auto PTAB  = e.tablet;
    const auto PTOOL = ensureTabletToolPresent(e.tool);

    PTOOL->active = e.in;

    if (!e.in) {
        if (PTOOL->getSurface())
            unfocusTool(PTOOL);
    } else {
        simulateMouseMovement();
        refocusTablet(PTAB, PTOOL);
    }

    PROTO::idle->onActivity();
}

void CInputManager::newTablet(wlr_input_device* pDevice) {
    const auto PNEWTABLET = m_vTablets.emplace_back(CTablet::create(wlr_tablet_from_input_device(pDevice)));
    m_vHIDs.push_back(PNEWTABLET);

    try {
        PNEWTABLET->hlName = deviceNameToInternalString(pDevice->name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Tablet had no name???"); // logic error
    }

    g_pPointerManager->attachTablet(PNEWTABLET);

    PNEWTABLET->events.destroy.registerStaticListener(
        [this](void* owner, std::any d) {
            auto TABLET = ((CTablet*)owner)->self;
            destroyTablet(TABLET.lock());
        },
        PNEWTABLET.get());

    setTabletConfigs();
}

SP<CTabletTool> CInputManager::ensureTabletToolPresent(wlr_tablet_tool* pTool) {
    if (pTool->data == nullptr) {
        const auto PTOOL = m_vTabletTools.emplace_back(CTabletTool::create(pTool));
        m_vHIDs.push_back(PTOOL);

        PTOOL->events.destroy.registerStaticListener(
            [this](void* owner, std::any d) {
                auto TOOL = ((CTabletTool*)owner)->self;
                destroyTabletTool(TOOL.lock());
            },
            PTOOL.get());
    }

    return CTabletTool::fromWlr(pTool);
}

void CInputManager::newTabletPad(wlr_input_device* pDevice) {
    const auto PNEWPAD = m_vTabletPads.emplace_back(CTabletPad::create(wlr_tablet_pad_from_input_device(pDevice)));
    m_vHIDs.push_back(PNEWPAD);

    try {
        PNEWPAD->hlName = deviceNameToInternalString(pDevice->name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Pad had no name???"); // logic error
    }

    // clang-format off
    PNEWPAD->events.destroy.registerStaticListener([this](void* owner, std::any d) {
        auto PAD = ((CTabletPad*)owner)->self;
        destroyTabletPad(PAD.lock());
    }, PNEWPAD.get());

    PNEWPAD->padEvents.button.registerStaticListener([this](void* owner, std::any e) {
        const auto E = std::any_cast<CTabletPad::SButtonEvent>(e);
        const auto PPAD  = ((CTabletPad*)owner)->self.lock();

        PROTO::tablet->mode(PPAD, 0, E.mode, E.timeMs);
        PROTO::tablet->buttonPad(PPAD, E.button, E.timeMs, E.down);
    }, PNEWPAD.get());

    PNEWPAD->padEvents.strip.registerStaticListener([this](void* owner, std::any e) {
        const auto E = std::any_cast<CTabletPad::SStripEvent>(e);
        const auto PPAD  = ((CTabletPad*)owner)->self.lock();

        PROTO::tablet->strip(PPAD, E.strip, E.position, E.finger, E.timeMs);
    }, PNEWPAD.get());

    PNEWPAD->padEvents.ring.registerStaticListener([this](void* owner, std::any e) {
        const auto E = std::any_cast<CTabletPad::SRingEvent>(e);
        const auto PPAD  = ((CTabletPad*)owner)->self.lock();

        PROTO::tablet->ring(PPAD, E.ring, E.position, E.finger, E.timeMs);
    }, PNEWPAD.get());

    PNEWPAD->padEvents.attach.registerStaticListener([this](void* owner, std::any e) {
        const auto PPAD  = ((CTabletPad*)owner)->self.lock();
        const auto TOOL = std::any_cast<SP<CTabletTool>>(e);

        PPAD->parent = TOOL;
    }, PNEWPAD.get());

    // clang-format on
}
