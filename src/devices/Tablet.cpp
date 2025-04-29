#include "Tablet.hpp"
#include "../defines.hpp"
#include "../protocols/Tablet.hpp"
#include "../protocols/core/Compositor.hpp"
#include <aquamarine/input/Input.hpp>

SP<CTablet> CTablet::create(SP<Aquamarine::ITablet> tablet) {
    SP<CTablet> pTab = SP<CTablet>(new CTablet(tablet));

    pTab->m_self = pTab;

    PROTO::tablet->registerDevice(pTab);

    return pTab;
}

SP<CTabletTool> CTabletTool::create(SP<Aquamarine::ITabletTool> tablet) {
    SP<CTabletTool> pTab = SP<CTabletTool>(new CTabletTool(tablet));

    pTab->m_self = pTab;

    PROTO::tablet->registerDevice(pTab);

    return pTab;
}

SP<CTabletPad> CTabletPad::create(SP<Aquamarine::ITabletPad> tablet) {
    SP<CTabletPad> pTab = SP<CTabletPad>(new CTabletPad(tablet));

    pTab->m_self = pTab;

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
    return m_tablet.lock();
}

CTablet::CTablet(SP<Aquamarine::ITablet> tablet_) : m_tablet(tablet_) {
    if (!m_tablet)
        return;

    m_listeners.destroy = m_tablet->events.destroy.registerListener([this](std::any d) {
        m_tablet.reset();
        m_events.destroy.emit();
    });

    m_listeners.axis = m_tablet->events.axis.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITablet::SAxisEvent>(d);

        m_tabletEvents.axis.emit(SAxisEvent{
            .tool        = E.tool,
            .tablet      = m_self.lock(),
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

    m_listeners.proximity = m_tablet->events.proximity.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITablet::SProximityEvent>(d);

        m_tabletEvents.proximity.emit(SProximityEvent{
            .tool      = E.tool,
            .tablet    = m_self.lock(),
            .timeMs    = E.timeMs,
            .proximity = E.absolute,
            .in        = E.in,
        });
    });

    m_listeners.tip = m_tablet->events.tip.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITablet::STipEvent>(d);

        m_tabletEvents.tip.emit(STipEvent{
            .tool   = E.tool,
            .tablet = m_self.lock(),
            .timeMs = E.timeMs,
            .tip    = E.absolute,
            .in     = E.down,
        });
    });

    m_listeners.button = m_tablet->events.button.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITablet::SButtonEvent>(d);

        m_tabletEvents.button.emit(SButtonEvent{
            .tool   = E.tool,
            .tablet = m_self.lock(),
            .timeMs = E.timeMs,
            .button = E.button,
            .down   = E.down,
        });
    });

    m_deviceName = m_tablet->getName();
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
    return m_pad.lock();
}

eHIDType CTabletPad::getType() {
    return HID_TYPE_TABLET_PAD;
}

CTabletPad::CTabletPad(SP<Aquamarine::ITabletPad> pad_) : m_pad(pad_) {
    if (!m_pad)
        return;

    m_listeners.destroy = m_pad->events.destroy.registerListener([this](std::any d) {
        m_pad.reset();
        m_events.destroy.emit();
    });

    m_listeners.button = m_pad->events.button.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITabletPad::SButtonEvent>(d);

        m_padEvents.button.emit(SButtonEvent{
            .timeMs = E.timeMs,
            .button = E.button,
            .down   = E.down,
            .mode   = E.mode,
            .group  = E.group,
        });
    });

    m_listeners.ring = m_pad->events.ring.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITabletPad::SRingEvent>(d);

        m_padEvents.ring.emit(SRingEvent{
            .timeMs   = E.timeMs,
            .finger   = E.source == Aquamarine::ITabletPad::AQ_TABLET_PAD_RING_SOURCE_FINGER,
            .ring     = E.ring,
            .position = E.pos,
            .mode     = E.mode,
        });
    });

    m_listeners.strip = m_pad->events.strip.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITabletPad::SStripEvent>(d);

        m_padEvents.strip.emit(SStripEvent{
            .timeMs   = E.timeMs,
            .finger   = E.source == Aquamarine::ITabletPad::AQ_TABLET_PAD_STRIP_SOURCE_FINGER,
            .strip    = E.strip,
            .position = E.pos,
            .mode     = E.mode,
        });
    });

    m_listeners.attach = m_pad->events.attach.registerListener([](std::any d) {
        ; // TODO: this doesn't do anything in aq atm
    });

    m_deviceName = m_pad->getName();
}

CTabletPad::~CTabletPad() {
    PROTO::tablet->recheckRegisteredDevices();
}

uint32_t CTabletTool::getCapabilities() {
    return HID_INPUT_CAPABILITY_POINTER | HID_INPUT_CAPABILITY_TABLET;
}

SP<Aquamarine::ITabletTool> CTabletTool::aq() {
    return m_tool.lock();
}

eHIDType CTabletTool::getType() {
    return HID_TYPE_TABLET_TOOL;
}

CTabletTool::CTabletTool(SP<Aquamarine::ITabletTool> tool_) : m_tool(tool_) {
    if (!m_tool)
        return;

    m_listeners.destroyTool = m_tool->events.destroy.registerListener([this](std::any d) {
        m_tool.reset();
        m_events.destroy.emit();
    });

    if (m_tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_TILT)
        m_toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_TILT;
    if (m_tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_PRESSURE)
        m_toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_PRESSURE;
    if (m_tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_DISTANCE)
        m_toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_DISTANCE;
    if (m_tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_ROTATION)
        m_toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_ROTATION;
    if (m_tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_SLIDER)
        m_toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_SLIDER;
    if (m_tool->capabilities & Aquamarine::ITabletTool::AQ_TABLET_TOOL_CAPABILITY_WHEEL)
        m_toolCapabilities |= HID_TABLET_TOOL_CAPABILITY_WHEEL;

    m_deviceName = std::format("{:x}-{:x}", m_tool->serial, m_tool->id);
}

CTabletTool::~CTabletTool() {
    PROTO::tablet->recheckRegisteredDevices();
}

SP<CWLSurfaceResource> CTabletTool::getSurface() {
    return m_surface.lock();
}

void CTabletTool::setSurface(SP<CWLSurfaceResource> surf) {
    if (surf == m_surface)
        return;

    if (m_surface) {
        m_listeners.destroySurface.reset();
        m_surface.reset();
    }

    m_surface = surf;

    if (surf) {
        m_listeners.destroySurface = surf->events.destroy.registerListener([this](std::any d) {
            PROTO::tablet->proximityOut(m_self.lock());
            m_surface.reset();
            m_listeners.destroySurface.reset();
        });
    }
}
