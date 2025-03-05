#include "Tablet.hpp"
#include "../defines.hpp"
#include "../protocols/Tablet.hpp"
#include "../protocols/core/Compositor.hpp"
#include <aquamarine/input/Input.hpp>

SP<CTablet> CTablet::create(SP<Aquamarine::ITablet> tablet) {
    SP<CTablet> pTab = SP<CTablet>(new CTablet(tablet));

    pTab->self = pTab;

    PROTO::tablet->registerDevice(pTab);

    return pTab;
}

SP<CTabletTool> CTabletTool::create(SP<Aquamarine::ITabletTool> tablet) {
    SP<CTabletTool> pTab = SP<CTabletTool>(new CTabletTool(tablet));

    pTab->self = pTab;

    PROTO::tablet->registerDevice(pTab);

    return pTab;
}

SP<CTabletPad> CTabletPad::create(SP<Aquamarine::ITabletPad> tablet) {
    SP<CTabletPad> pTab = SP<CTabletPad>(new CTabletPad(tablet));

    pTab->self = pTab;

    PROTO::tablet->registerDevice(pTab);

    return pTab;
}

static uint32_t aqUpdateToHl(uint32_t aq) {
    uint32_t result = 0;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_X)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_X;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_Y)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_Y;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_DISTANCE)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_DISTANCE;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_PRESSURE)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_PRESSURE;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_TILT_X)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_TILT_X;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_TILT_Y)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_TILT_Y;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_ROTATION)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_ROTATION;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_SLIDER)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_SLIDER;
    if (aq & Aquamarine::AQ_TABLET_TOOL_AXIS_WHEEL)
        result |= CTablet::eTabletToolAxes::HID_TABLET_TOOL_AXIS_WHEEL;
    return result;
}

uint32_t CTablet::getCapabilities() {
    return HID_INPUT_CAPABILITY_POINTER | HID_INPUT_CAPABILITY_TABLET;
}

SP<Aquamarine::ITablet> CTablet::aq() {
    return tablet.lock();
}

CTablet::CTablet(SP<Aquamarine::ITablet> tablet_) : tablet(tablet_) {
    if (!tablet)
        return;

    m_listeners.destroy = tablet->events.destroy.registerListener([this](std::any d) {
        tablet.reset();
        events.destroy.emit();
    });

    m_listeners.axis = tablet->events.axis.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITablet::SAxisEvent>(d);

        tabletEvents.axis.emit(SAxisEvent{
            .tool        = E.tool,
            .tablet      = self.lock(),
            .timeMs      = E.timeMs,
            .updatedAxes = aqUpdateToHl(E.updatedAxes),
            .axis        = E.absolute,
            .axisDelta   = E.delta,
            .tilt        = E.tilt,
            .pressure    = E.pressure,
            .distance    = E.distance,
            .rotation    = E.rotation,
            .slider      = E.slider,
            .wheelDelta  = E.wheelDelta,
        });
    });

    m_listeners.proximity = tablet->events.proximity.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITablet::SProximityEvent>(d);

        tabletEvents.proximity.emit(SProximityEvent{
            .tool      = E.tool,
            .tablet    = self.lock(),
            .timeMs    = E.timeMs,
            .proximity = E.absolute,
            .in        = E.in,
        });
    });

    m_listeners.tip = tablet->events.tip.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITablet::STipEvent>(d);

        tabletEvents.tip.emit(STipEvent{
            .tool   = E.tool,
            .tablet = self.lock(),
            .timeMs = E.timeMs,
            .tip    = E.absolute,
            .in     = E.down,
        });
    });

    m_listeners.button = tablet->events.button.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITablet::SButtonEvent>(d);

        tabletEvents.button.emit(SButtonEvent{
            .tool   = E.tool,
            .tablet = self.lock(),
            .timeMs = E.timeMs,
            .button = E.button,
            .down   = E.down,
        });
    });

    deviceName = tablet->getName();
}

CTablet::~CTablet() {
    PROTO::tablet->recheckRegisteredDevices();
}

eHIDType CTablet::getType() {
    return HID_TYPE_TABLET;
}

uint32_t CTabletPad::getCapabilities() {
    return HID_INPUT_CAPABILITY_TABLET;
}

SP<Aquamarine::ITabletPad> CTabletPad::aq() {
    return pad.lock();
}

eHIDType CTabletPad::getType() {
    return HID_TYPE_TABLET_PAD;
}

CTabletPad::CTabletPad(SP<Aquamarine::ITabletPad> pad_) : pad(pad_) {
    if (!pad)
        return;

    m_listeners.destroy = pad->events.destroy.registerListener([this](std::any d) {
        pad.reset();
        events.destroy.emit();
    });

    m_listeners.button = pad->events.button.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITabletPad::SButtonEvent>(d);

        padEvents.button.emit(SButtonEvent{
            .timeMs = E.timeMs,
            .button = E.button,
            .down   = E.down,
            .mode   = E.mode,
            .group  = E.group,
        });
    });

    m_listeners.ring = pad->events.ring.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITabletPad::SRingEvent>(d);

        padEvents.ring.emit(SRingEvent{
            .timeMs   = E.timeMs,
            .finger   = E.source == Aquamarine::ITabletPad::AQ_TABLET_PAD_RING_SOURCE_FINGER,
            .ring     = E.ring,
            .position = E.pos,
            .mode     = E.mode,
        });
    });

    m_listeners.strip = pad->events.strip.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITabletPad::SStripEvent>(d);

        padEvents.strip.emit(SStripEvent{
            .timeMs   = E.timeMs,
            .finger   = E.source == Aquamarine::ITabletPad::AQ_TABLET_PAD_STRIP_SOURCE_FINGER,
            .strip    = E.strip,
            .position = E.pos,
            .mode     = E.mode,
        });
    });

    m_listeners.attach = pad->events.attach.registerListener([](std::any d) {
        ; // TODO: this doesn't do anything in aq atm
    });

    deviceName = pad->getName();
}

CTabletPad::~CTabletPad() {
    PROTO::tablet->recheckRegisteredDevices();
}

uint32_t CTabletTool::getCapabilities() {
    return HID_INPUT_CAPABILITY_POINTER | HID_INPUT_CAPABILITY_TABLET;
}

SP<Aquamarine::ITabletTool> CTabletTool::aq() {
    return tool.lock();
}

eHIDType CTabletTool::getType() {
    return HID_TYPE_TABLET_TOOL;
}

CTabletTool::CTabletTool(SP<Aquamarine::ITabletTool> tool_) : tool(tool_) {
    if (!tool)
        return;

    m_listeners.destroyTool = tool->events.destroy.registerListener([this](std::any d) {
        tool.reset();
        events.destroy.emit();
    });

    if (tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_TILT)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_TILT;
    if (tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_PRESSURE)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_PRESSURE;
    if (tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_DISTANCE)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_DISTANCE;
    if (tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_ROTATION)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_ROTATION;
    if (tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_SLIDER)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_SLIDER;
    if (tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_WHEEL)
        toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_WHEEL;

    deviceName = std::format("{:x}-{:x}", tool->serial, tool->id);
}

CTabletTool::~CTabletTool() {
    PROTO::tablet->recheckRegisteredDevices();
}

SP<CWLSurfaceResource> CTabletTool::getSurface() {
    return pSurface.lock();
}

void CTabletTool::setSurface(SP<CWLSurfaceResource> surf) {
    if (surf == pSurface)
        return;

    if (pSurface) {
        m_listeners.destroySurface.reset();
        pSurface.reset();
    }

    pSurface = surf;

    if (surf) {
        m_listeners.destroySurface = surf->events.destroy.registerListener([this](std::any d) {
            PROTO::tablet->proximityOut(self.lock());
            pSurface.reset();
            m_listeners.destroySurface.reset();
        });
    }
}
