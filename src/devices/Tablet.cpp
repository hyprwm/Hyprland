#include "Tablet.hpp"
#include "../defines.hpp"
#include "../protocols/Tablet.hpp"
#include "../protocols/core/Compositor.hpp"

SP<CTablet> CTablet::create(wlr_tablet* tablet) {
    SP<CTablet> pTab = SP<CTablet>(new CTablet(tablet));

    pTab->self = pTab;

    PROTO::tablet->registerDevice(pTab);

    return pTab;
}

SP<CTabletTool> CTabletTool::create(wlr_tablet_tool* tablet) {
    SP<CTabletTool> pTab = SP<CTabletTool>(new CTabletTool(tablet));

    pTab->self = pTab;

    PROTO::tablet->registerDevice(pTab);

    return pTab;
}

SP<CTabletPad> CTabletPad::create(wlr_tablet_pad* tablet) {
    SP<CTabletPad> pTab = SP<CTabletPad>(new CTabletPad(tablet));

    pTab->self = pTab;

    PROTO::tablet->registerDevice(pTab);

    return pTab;
}

SP<CTabletTool> CTabletTool::fromWlr(wlr_tablet_tool* tool) {
    return ((CTabletTool*)tool->data)->self.lock();
}

SP<CTablet> CTablet::fromWlr(wlr_tablet* tablet) {
    return ((CTablet*)tablet->data)->self.lock();
}

static uint32_t wlrUpdateToHl(uint32_t wlr) {
    uint32_t result = 0;
    if (wlr & WLR_TABLET_TOOL_AXIS_X)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_X;
    if (wlr & WLR_TABLET_TOOL_AXIS_Y)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_Y;
    if (wlr & WLR_TABLET_TOOL_AXIS_DISTANCE)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_DISTANCE;
    if (wlr & WLR_TABLET_TOOL_AXIS_PRESSURE)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_PRESSURE;
    if (wlr & WLR_TABLET_TOOL_AXIS_TILT_X)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_TILT_X;
    if (wlr & WLR_TABLET_TOOL_AXIS_TILT_Y)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_TILT_Y;
    if (wlr & WLR_TABLET_TOOL_AXIS_ROTATION)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_ROTATION;
    if (wlr & WLR_TABLET_TOOL_AXIS_SLIDER)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_SLIDER;
    if (wlr & WLR_TABLET_TOOL_AXIS_WHEEL)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_WHEEL;
    return result;
}

uint32_t CTablet::getCapabilities() {
    return HID_INPUT_CAPABILITY_POINTER | HID_INPUT_CAPABILITY_TABLET;
}

wlr_tablet* CTablet::wlr() {
    return tablet;
}

CTablet::CTablet(wlr_tablet* tablet_) : tablet(tablet_) {
    if (!tablet)
        return;

    tablet->data = this;

    // clang-format off
    hyprListener_destroy.initCallback(&tablet->base.events.destroy, [this] (void* owner, void* data) {
        tablet = nullptr;
        disconnectCallbacks();
        events.destroy.emit();
    }, this, "CTablet");

    hyprListener_axis.initCallback(&tablet->events.axis, [this] (void* owner, void* data) {
        auto E = (wlr_tablet_tool_axis_event*)data;

        tabletEvents.axis.emit(SAxisEvent{
            .tool        = E->tool,
            .tablet      = self.lock(),
            .timeMs      = E->time_msec,
            .updatedAxes = wlrUpdateToHl(E->updated_axes),
            .axis        = {E->x, E->y},
            .axisDelta   = {E->dx, E->dy},
            .tilt        = {E->tilt_x, E->tilt_y},
            .pressure    = E->pressure,
            .distance    = E->distance,
            .rotation    = E->rotation,
            .slider      = E->slider,
            .wheelDelta  = E->wheel_delta,
        });
    }, this, "CTablet");

    hyprListener_proximity.initCallback(&tablet->events.proximity, [this] (void* owner, void* data) {
        auto E = (wlr_tablet_tool_proximity_event*)data;

        tabletEvents.proximity.emit(SProximityEvent{
            .tool      = E->tool,
            .tablet    = self.lock(),
            .timeMs    = E->time_msec,
            .proximity = {E->x, E->y},
            .in        = E->state == WLR_TABLET_TOOL_PROXIMITY_IN,
        });
    }, this, "CTablet");

    hyprListener_tip.initCallback(&tablet->events.tip, [this] (void* owner, void* data) {
        auto E = (wlr_tablet_tool_tip_event*)data;

        tabletEvents.tip.emit(STipEvent{
            .tool   = E->tool,
            .tablet = self.lock(),
            .timeMs = E->time_msec,
            .tip    = {E->x, E->y},
            .in     = E->state == WLR_TABLET_TOOL_TIP_DOWN,
        });
    }, this, "CTablet");

    hyprListener_button.initCallback(&tablet->events.button, [this] (void* owner, void* data) {
        auto E = (wlr_tablet_tool_button_event*)data;

        tabletEvents.button.emit(SButtonEvent{
            .tool   = E->tool,
            .tablet = self.lock(),
            .timeMs = E->time_msec,
            .button = E->button,
            .down   = E->state == WLR_BUTTON_PRESSED,
        });
    }, this, "CTablet");
    // clang-format on

    deviceName = tablet->base.name ? tablet->base.name : "UNKNOWN";
}

CTablet::~CTablet() {
    if (tablet)
        tablet->data = nullptr;

    PROTO::tablet->recheckRegisteredDevices();
}

void CTablet::disconnectCallbacks() {
    hyprListener_axis.removeCallback();
    hyprListener_button.removeCallback();
    hyprListener_destroy.removeCallback();
    hyprListener_proximity.removeCallback();
    hyprListener_tip.removeCallback();
}

eHIDType CTablet::getType() {
    return HID_TYPE_TABLET;
}

uint32_t CTabletPad::getCapabilities() {
    return HID_INPUT_CAPABILITY_TABLET;
}

wlr_tablet_pad* CTabletPad::wlr() {
    return pad;
}

eHIDType CTabletPad::getType() {
    return HID_TYPE_TABLET_PAD;
}

CTabletPad::CTabletPad(wlr_tablet_pad* pad_) : pad(pad_) {
    if (!pad)
        return;

    // clang-format off
    hyprListener_destroy.initCallback(&pad->base.events.destroy, [this] (void* owner, void* data) {
        pad = nullptr;
        disconnectCallbacks();
        events.destroy.emit();
    }, this, "CTabletPad");

    hyprListener_button.initCallback(&pad->events.button, [this] (void* owner, void* data) {
        auto E = (wlr_tablet_pad_button_event*)data;

        padEvents.button.emit(SButtonEvent{
            .timeMs = E->time_msec,
            .button = E->button,
            .down   = E->state == WLR_BUTTON_PRESSED,
            .mode   = E->mode,
            .group  = E->group,
        });
    }, this, "CTabletPad");

    hyprListener_ring.initCallback(&pad->events.ring, [this] (void* owner, void* data) {
        auto E = (wlr_tablet_pad_ring_event*)data;

        padEvents.ring.emit(SRingEvent{
            .timeMs   = E->time_msec,
            .finger   = E->source == WLR_TABLET_PAD_RING_SOURCE_FINGER,
            .ring     = E->ring,
            .position = E->position,
            .mode     = E->mode,
        });
    }, this, "CTabletPad");

    hyprListener_strip.initCallback(&pad->events.strip, [this] (void* owner, void* data) {
        auto E = (wlr_tablet_pad_strip_event*)data;

        padEvents.strip.emit(SStripEvent{
            .timeMs   = E->time_msec,
            .finger   = E->source == WLR_TABLET_PAD_STRIP_SOURCE_FINGER,
            .strip    = E->strip,
            .position = E->position,
            .mode     = E->mode,
        });
    }, this, "CTabletPad");

    hyprListener_attach.initCallback(&pad->events.attach_tablet, [this] (void* owner, void* data) {
        if (!data)
            return;
            
        padEvents.attach.emit(CTabletTool::fromWlr((wlr_tablet_tool*)data));
    }, this, "CTabletPad");
    // clang-format on

    deviceName = pad->base.name ? pad->base.name : "UNKNOWN";
}

CTabletPad::~CTabletPad() {
    PROTO::tablet->recheckRegisteredDevices();
}

void CTabletPad::disconnectCallbacks() {
    hyprListener_ring.removeCallback();
    hyprListener_button.removeCallback();
    hyprListener_destroy.removeCallback();
    hyprListener_strip.removeCallback();
    hyprListener_attach.removeCallback();
}

uint32_t CTabletTool::getCapabilities() {
    return HID_INPUT_CAPABILITY_POINTER | HID_INPUT_CAPABILITY_TABLET;
}

wlr_tablet_tool* CTabletTool::wlr() {
    return tool;
}

eHIDType CTabletTool::getType() {
    return HID_TYPE_TABLET_TOOL;
}

CTabletTool::CTabletTool(wlr_tablet_tool* tool_) : tool(tool_) {
    if (!tool)
        return;

    // clang-format off
    hyprListener_destroy.initCallback(&tool->events.destroy, [this] (void* owner, void* data) {
        tool = nullptr;
        disconnectCallbacks();
        events.destroy.emit();
    }, this, "CTabletTool");
    // clang-format on

    if (tool->tilt)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_TILT;
    if (tool->pressure)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_PRESSURE;
    if (tool->distance)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_DISTANCE;
    if (tool->rotation)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_ROTATION;
    if (tool->slider)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_SLIDER;
    if (tool->wheel)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_WHEEL;

    tool->data = this;

    deviceName = std::to_string(tool->hardware_serial) + std::to_string(tool->hardware_wacom);
}

CTabletTool::~CTabletTool() {
    if (tool)
        tool->data = nullptr;

    PROTO::tablet->recheckRegisteredDevices();
}

void CTabletTool::disconnectCallbacks() {
    hyprListener_destroy.removeCallback();
    listeners.destroySurface.reset();
}

SP<CWLSurfaceResource> CTabletTool::getSurface() {
    return pSurface.lock();
}

void CTabletTool::setSurface(SP<CWLSurfaceResource> surf) {
    if (surf == pSurface)
        return;

    if (pSurface) {
        listeners.destroySurface.reset();
        pSurface.reset();
    }

    pSurface = surf;

    if (surf) {
        listeners.destroySurface = surf->events.destroy.registerListener([this](std::any d) {
            PROTO::tablet->proximityOut(self.lock());
            pSurface.reset();
            listeners.destroySurface.reset();
        });
    }
}
