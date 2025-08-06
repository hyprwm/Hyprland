#include "Tablet.hpp"
#include "../devices/Tablet.hpp"
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../helpers/time/Time.hpp"
#include "core/Seat.hpp"
#include "core/Compositor.hpp"
#include <algorithm>
#include <cstring>

CTabletPadStripV2Resource::CTabletPadStripV2Resource(SP<CZwpTabletPadStripV2> resource_, uint32_t id_) : m_id(id_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpTabletPadStripV2* r) { PROTO::tablet->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpTabletPadStripV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletPadStripV2Resource::good() {
    return m_resource->resource();
}

CTabletPadRingV2Resource::CTabletPadRingV2Resource(SP<CZwpTabletPadRingV2> resource_, uint32_t id_) : m_id(id_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpTabletPadRingV2* r) { PROTO::tablet->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpTabletPadRingV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletPadRingV2Resource::good() {
    return m_resource->resource();
}

CTabletPadGroupV2Resource::CTabletPadGroupV2Resource(SP<CZwpTabletPadGroupV2> resource_, size_t idx_) : m_idx(idx_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpTabletPadGroupV2* r) { PROTO::tablet->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpTabletPadGroupV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletPadGroupV2Resource::good() {
    return m_resource->resource();
}

void CTabletPadGroupV2Resource::sendData(SP<CTabletPad> pad, SP<Aquamarine::ITabletPad::STabletPadGroup> group) {
    m_resource->sendModes(group->modes);

    wl_array buttonArr;
    wl_array_init(&buttonArr);
    wl_array_add(&buttonArr, group->buttons.size() * sizeof(int));
    memcpy(buttonArr.data, group->buttons.data(), group->buttons.size() * sizeof(int));
    m_resource->sendButtons(&buttonArr);
    wl_array_release(&buttonArr);

    for (size_t i = 0; i < group->strips.size(); ++i) {
        const auto RESOURCE =
            PROTO::tablet->m_strips.emplace_back(makeShared<CTabletPadStripV2Resource>(makeShared<CZwpTabletPadStripV2>(m_resource->client(), m_resource->version(), 0), i));

        if UNLIKELY (!RESOURCE->good()) {
            m_resource->noMemory();
            PROTO::tablet->m_strips.pop_back();
            return;
        }

        m_resource->sendStrip(RESOURCE->m_resource.get());
    }

    for (size_t i = 0; i < group->rings.size(); ++i) {
        const auto RESOURCE =
            PROTO::tablet->m_rings.emplace_back(makeShared<CTabletPadRingV2Resource>(makeShared<CZwpTabletPadRingV2>(m_resource->client(), m_resource->version(), 0), i));

        if UNLIKELY (!RESOURCE->good()) {
            m_resource->noMemory();
            PROTO::tablet->m_rings.pop_back();
            return;
        }

        m_resource->sendRing(RESOURCE->m_resource.get());
    }

    m_resource->sendDone();
}

CTabletPadV2Resource::CTabletPadV2Resource(SP<CZwpTabletPadV2> resource_, SP<CTabletPad> pad_, SP<CTabletSeat> seat_) : m_pad(pad_), m_seat(seat_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpTabletPadV2* r) { PROTO::tablet->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpTabletPadV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletPadV2Resource::good() {
    return m_resource->resource();
}

void CTabletPadV2Resource::sendData() {
    // this is dodgy as fuck. I hate wl_array. it's expanded wl_array_for_each because C++ would complain about the implicit casts
    for (auto const& p : m_pad->aq()->paths) {
        m_resource->sendPath(p.c_str());
    }

    m_resource->sendButtons(m_pad->aq()->buttons);

    for (size_t i = 0; i < m_pad->aq()->groups.size(); ++i) {
        createGroup(m_pad->aq()->groups.at(i), i);
    }

    m_resource->sendDone();
}

void CTabletPadV2Resource::createGroup(SP<Aquamarine::ITabletPad::STabletPadGroup> group, size_t idx) {
    const auto RESOURCE =
        PROTO::tablet->m_groups.emplace_back(makeShared<CTabletPadGroupV2Resource>(makeShared<CZwpTabletPadGroupV2>(m_resource->client(), m_resource->version(), 0), idx));

    if UNLIKELY (!RESOURCE->good()) {
        m_resource->noMemory();
        PROTO::tablet->m_groups.pop_back();
        return;
    }

    m_resource->sendGroup(RESOURCE->m_resource.get());

    RESOURCE->sendData(m_pad.lock(), group);
}

CTabletV2Resource::CTabletV2Resource(SP<CZwpTabletV2> resource_, SP<CTablet> tablet_, SP<CTabletSeat> seat_) : m_tablet(tablet_), m_seat(seat_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpTabletV2* r) { PROTO::tablet->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpTabletV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletV2Resource::good() {
    return m_resource->resource();
}

void CTabletV2Resource::sendData() {
    m_resource->sendName(m_tablet->m_deviceName.c_str());
    m_resource->sendId(m_tablet->aq()->usbVendorID, m_tablet->aq()->usbProductID);

    for (auto const& p : m_tablet->aq()->paths) {
        m_resource->sendPath(p.c_str());
    }

    m_resource->sendDone();
}

CTabletToolV2Resource::CTabletToolV2Resource(SP<CZwpTabletToolV2> resource_, SP<CTabletTool> tool_, SP<CTabletSeat> seat_) : m_tool(tool_), m_seat(seat_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpTabletToolV2* r) { PROTO::tablet->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpTabletToolV2* r) { PROTO::tablet->destroyResource(this); });

    m_resource->setSetCursor([](CZwpTabletToolV2* r, uint32_t serial, wl_resource* surf, int32_t hot_x, int32_t hot_y) {
        if (!g_pSeatManager->m_state.pointerFocusResource || g_pSeatManager->m_state.pointerFocusResource->client() != r->client())
            return;

        g_pInputManager->processMouseRequest(CSeatManager::SSetCursorEvent{surf ? CWLSurfaceResource::fromResource(surf) : nullptr, {hot_x, hot_y}});
    });
}

CTabletToolV2Resource::~CTabletToolV2Resource() {
    if (m_frameSource)
        wl_event_source_remove(m_frameSource);
}

bool CTabletToolV2Resource::good() {
    return m_resource->resource();
}

void CTabletToolV2Resource::sendData() {
    static auto AQ_TYPE_TO_PROTO = [](uint32_t aq) -> zwpTabletToolV2Type {
        switch (aq) {
            case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_PEN: return ZWP_TABLET_TOOL_V2_TYPE_PEN;
            case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_ERASER: return ZWP_TABLET_TOOL_V2_TYPE_ERASER;
            case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_BRUSH: return ZWP_TABLET_TOOL_V2_TYPE_BRUSH;
            case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_PENCIL: return ZWP_TABLET_TOOL_V2_TYPE_PENCIL;
            case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_AIRBRUSH: return ZWP_TABLET_TOOL_V2_TYPE_AIRBRUSH;
            case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_MOUSE: return ZWP_TABLET_TOOL_V2_TYPE_MOUSE;
            case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_LENS: return ZWP_TABLET_TOOL_V2_TYPE_LENS;
            default: ASSERT(false);
        }
        UNREACHABLE();
    };

    m_resource->sendType(AQ_TYPE_TO_PROTO(m_tool->aq()->type));
    m_resource->sendHardwareSerial(m_tool->aq()->serial >> 32, m_tool->aq()->serial & 0xFFFFFFFF);
    m_resource->sendHardwareIdWacom(m_tool->aq()->id >> 32, m_tool->aq()->id & 0xFFFFFFFF);
    if (m_tool->m_toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_DISTANCE)
        m_resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_DISTANCE);
    if (m_tool->m_toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_PRESSURE)
        m_resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE);
    if (m_tool->m_toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_ROTATION)
        m_resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_ROTATION);
    if (m_tool->m_toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_SLIDER)
        m_resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_SLIDER);
    if (m_tool->m_toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_TILT)
        m_resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_TILT);
    if (m_tool->m_toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_WHEEL)
        m_resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_WHEEL);
    m_resource->sendDone();
}

void CTabletToolV2Resource::queueFrame() {
    if (m_frameSource)
        return;

    m_frameSource = wl_event_loop_add_idle(g_pCompositor->m_wlEventLoop, [](void* data) { static_cast<CTabletToolV2Resource*>(data)->sendFrame(false); }, this);
}

void CTabletToolV2Resource::sendFrame(bool removeSource) {
    if (m_frameSource) {
        if (removeSource)
            wl_event_source_remove(m_frameSource);
        m_frameSource = nullptr;
    }

    if (!m_current)
        return;

    m_resource->sendFrame(Time::millis(Time::steadyNow()));
}

CTabletSeat::CTabletSeat(SP<CZwpTabletSeatV2> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpTabletSeatV2* r) { PROTO::tablet->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpTabletSeatV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletSeat::good() {
    return m_resource->resource();
}

void CTabletSeat::sendTool(SP<CTabletTool> tool) {
    const auto RESOURCE =
        PROTO::tablet->m_tools.emplace_back(makeShared<CTabletToolV2Resource>(makeShared<CZwpTabletToolV2>(m_resource->client(), m_resource->version(), 0), tool, m_self.lock()));

    if UNLIKELY (!RESOURCE->good()) {
        m_resource->noMemory();
        PROTO::tablet->m_tools.pop_back();
        return;
    }

    m_resource->sendToolAdded(RESOURCE->m_resource.get());

    RESOURCE->sendData();
    m_tools.emplace_back(RESOURCE);
}

void CTabletSeat::sendPad(SP<CTabletPad> pad) {
    const auto RESOURCE =
        PROTO::tablet->m_pads.emplace_back(makeShared<CTabletPadV2Resource>(makeShared<CZwpTabletPadV2>(m_resource->client(), m_resource->version(), 0), pad, m_self.lock()));

    if UNLIKELY (!RESOURCE->good()) {
        m_resource->noMemory();
        PROTO::tablet->m_pads.pop_back();
        return;
    }

    m_resource->sendPadAdded(RESOURCE->m_resource.get());

    RESOURCE->sendData();
    m_pads.emplace_back(RESOURCE);
}

void CTabletSeat::sendTablet(SP<CTablet> tablet) {
    const auto RESOURCE =
        PROTO::tablet->m_tablets.emplace_back(makeShared<CTabletV2Resource>(makeShared<CZwpTabletV2>(m_resource->client(), m_resource->version(), 0), tablet, m_self.lock()));

    if UNLIKELY (!RESOURCE->good()) {
        m_resource->noMemory();
        PROTO::tablet->m_tablets.pop_back();
        return;
    }

    m_resource->sendTabletAdded(RESOURCE->m_resource.get());

    RESOURCE->sendData();
    m_tablets.emplace_back(RESOURCE);
}

void CTabletSeat::sendData() {
    for (auto const& tw : PROTO::tablet->m_tabletDevices) {
        if (tw.expired())
            continue;

        sendTablet(tw.lock());
    }

    for (auto const& tw : PROTO::tablet->m_toolDevices) {
        if (tw.expired())
            continue;

        sendTool(tw.lock());
    }

    for (auto const& tw : PROTO::tablet->m_padDevices) {
        if (tw.expired())
            continue;

        sendPad(tw.lock());
    }
}

CTabletV2Protocol::CTabletV2Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CTabletV2Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwpTabletManagerV2>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpTabletManagerV2* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpTabletManagerV2* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetTabletSeat([this](CZwpTabletManagerV2* pMgr, uint32_t id, wl_resource* seat) { this->onGetSeat(pMgr, id, seat); });
}

void CTabletV2Protocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CTabletV2Protocol::destroyResource(CTabletSeat* resource) {
    std::erase_if(m_seats, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletToolV2Resource* resource) {
    std::erase_if(m_tools, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletV2Resource* resource) {
    std::erase_if(m_tablets, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletPadV2Resource* resource) {
    std::erase_if(m_pads, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletPadGroupV2Resource* resource) {
    std::erase_if(m_groups, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletPadRingV2Resource* resource) {
    std::erase_if(m_rings, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletPadStripV2Resource* resource) {
    std::erase_if(m_strips, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::onGetSeat(CZwpTabletManagerV2* pMgr, uint32_t id, wl_resource* seat) {
    const auto RESOURCE = m_seats.emplace_back(makeShared<CTabletSeat>(makeShared<CZwpTabletSeatV2>(pMgr->client(), pMgr->version(), id)));

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_seats.pop_back();
        return;
    }

    RESOURCE->m_self = RESOURCE;
    RESOURCE->sendData();
}

void CTabletV2Protocol::registerDevice(SP<CTablet> tablet) {
    for (auto const& s : m_seats) {
        s->sendTablet(tablet);
    }

    m_tabletDevices.emplace_back(tablet);
}

void CTabletV2Protocol::registerDevice(SP<CTabletTool> tool) {
    for (auto const& s : m_seats) {
        s->sendTool(tool);
    }

    m_toolDevices.emplace_back(tool);
}

void CTabletV2Protocol::registerDevice(SP<CTabletPad> pad) {
    for (auto const& s : m_seats) {
        s->sendPad(pad);
    }

    m_padDevices.emplace_back(pad);
}

void CTabletV2Protocol::unregisterDevice(SP<CTablet> tablet) {
    for (auto const& t : m_tablets) {
        if (t->m_tablet == tablet) {
            t->m_resource->sendRemoved();
            t->m_inert = true;
        }
    }
    std::erase_if(m_tabletDevices, [tablet](const auto& e) { return e.expired() || e == tablet; });
}

void CTabletV2Protocol::unregisterDevice(SP<CTabletTool> tool) {
    for (auto const& t : m_tools) {
        if (t->m_tool == tool) {
            t->m_resource->sendRemoved();
            t->m_inert = true;
        }
    }
    std::erase_if(m_toolDevices, [tool](const auto& e) { return e.expired() || e == tool; });
}

void CTabletV2Protocol::unregisterDevice(SP<CTabletPad> pad) {
    for (auto const& t : m_pads) {
        if (t->m_pad == pad) {
            t->m_resource->sendRemoved();
            t->m_inert = true;
        }
    }
    std::erase_if(m_padDevices, [pad](const auto& e) { return e.expired() || e == pad; });
}

void CTabletV2Protocol::recheckRegisteredDevices() {
    std::erase_if(m_tabletDevices, [](const auto& e) { return e.expired(); });
    std::erase_if(m_toolDevices, [](const auto& e) { return e.expired(); });
    std::erase_if(m_padDevices, [](const auto& e) { return e.expired(); });

    // now we need to send removed events
    for (auto const& t : m_tablets) {
        if (!t->m_tablet.expired() || t->m_inert)
            continue;

        t->m_resource->sendRemoved();
        t->m_inert = true;
    }

    for (auto const& t : m_tools) {
        if (!t->m_tool.expired() || t->m_inert)
            continue;

        if (t->m_current) {
            t->m_resource->sendProximityOut();
            t->sendFrame();
            t->m_lastSurf.reset();
        }

        t->m_resource->sendRemoved();
        t->m_inert = true;
    }

    for (auto const& t : m_pads) {
        if (!t->m_pad.expired() || t->m_inert)
            continue;

        t->m_resource->sendRemoved();
        t->m_inert = true;
    }
}

void CTabletV2Protocol::pressure(SP<CTabletTool> tool, double value) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_resource->sendPressure(std::clamp(value * 65535, 0.0, 65535.0));
        t->queueFrame();
    }
}

void CTabletV2Protocol::distance(SP<CTabletTool> tool, double value) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_resource->sendDistance(std::clamp(value * 65535, 0.0, 65535.0));
        t->queueFrame();
    }
}

void CTabletV2Protocol::rotation(SP<CTabletTool> tool, double value) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_resource->sendRotation(wl_fixed_from_double(value));
        t->queueFrame();
    }
}

void CTabletV2Protocol::slider(SP<CTabletTool> tool, double value) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_resource->sendSlider(std::clamp(value * 65535, -65535.0, 65535.0));
        t->queueFrame();
    }
}

void CTabletV2Protocol::wheel(SP<CTabletTool> tool, double value) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_resource->sendWheel(wl_fixed_from_double(value), 0);
        t->queueFrame();
    }
}

void CTabletV2Protocol::tilt(SP<CTabletTool> tool, const Vector2D& value) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_resource->sendTilt(wl_fixed_from_double(value.x), wl_fixed_from_double(value.y));
        t->queueFrame();
    }
}

void CTabletV2Protocol::up(SP<CTabletTool> tool) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_resource->sendUp();
        t->queueFrame();
    }
}

void CTabletV2Protocol::down(SP<CTabletTool> tool) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        auto serial = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(t->m_resource->client()));
        t->m_resource->sendDown(serial);
        t->queueFrame();
    }
}

void CTabletV2Protocol::proximityIn(SP<CTabletTool> tool, SP<CTablet> tablet, SP<CWLSurfaceResource> surf) {
    proximityOut(tool);
    const auto                CLIENT = surf->client();

    SP<CTabletToolV2Resource> toolResource;
    SP<CTabletV2Resource>     tabletResource;

    for (auto const& t : m_tools) {
        if (t->m_tool != tool || t->m_resource->client() != CLIENT)
            continue;

        if (t->m_seat.expired()) {
            LOGM(ERR, "proximityIn on a tool without a seat parent");
            return;
        }

        if (t->m_lastSurf == surf)
            return;

        toolResource = t;

        for (auto const& tab : m_tablets) {
            if (tab->m_tablet != tablet)
                continue;

            if (tab->m_seat != t->m_seat || !tab->m_seat)
                continue;

            tabletResource = tab;
            break;
        }
    }

    if (!tabletResource || !toolResource) {
        LOGM(ERR, "proximityIn on a tool and tablet without valid resource(s)??");
        return;
    }

    toolResource->m_current  = true;
    toolResource->m_lastSurf = surf;

    auto serial = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(toolResource->m_resource->client()));
    toolResource->m_resource->sendProximityIn(serial, tabletResource->m_resource.get(), surf->getResource()->resource());
    toolResource->queueFrame();

    LOGM(ERR, "proximityIn: found no resource to send enter");
}

void CTabletV2Protocol::proximityOut(SP<CTabletTool> tool) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_lastSurf.reset();
        t->m_resource->sendProximityOut();
        t->sendFrame();
        t->m_current = false;
    }
}

void CTabletV2Protocol::buttonTool(SP<CTabletTool> tool, uint32_t button, uint32_t state) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        auto serial = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(t->m_resource->client()));
        t->m_resource->sendButton(serial, button, static_cast<zwpTabletToolV2ButtonState>(state));
        t->queueFrame();
    }
}

void CTabletV2Protocol::motion(SP<CTabletTool> tool, const Vector2D& value) {
    for (auto const& t : m_tools) {
        if (t->m_tool != tool || !t->m_current)
            continue;

        t->m_resource->sendMotion(wl_fixed_from_double(value.x), wl_fixed_from_double(value.y));
        t->queueFrame();
    }
}

void CTabletV2Protocol::mode(SP<CTabletPad> pad, uint32_t group, uint32_t mode, uint32_t timeMs) {
    for (auto const& t : m_pads) {
        if (t->m_pad != pad)
            continue;
        if (t->m_groups.size() <= group) {
            LOGM(ERR, "BUG THIS: group >= t->groups.size()");
            return;
        }
        auto serial = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(t->m_resource->client()));
        t->m_groups.at(group)->m_resource->sendModeSwitch(timeMs, serial, mode);
    }
}

void CTabletV2Protocol::buttonPad(SP<CTabletPad> pad, uint32_t button, uint32_t timeMs, uint32_t state) {
    for (auto const& t : m_pads) {
        if (t->m_pad != pad)
            continue;
        t->m_resource->sendButton(timeMs, button, zwpTabletToolV2ButtonState{state});
    }
}

void CTabletV2Protocol::strip(SP<CTabletPad> pad, uint32_t strip, double position, bool finger, uint32_t timeMs) {
    LOGM(ERR, "FIXME: STUB: CTabletV2Protocol::strip not implemented");
}

void CTabletV2Protocol::ring(SP<CTabletPad> pad, uint32_t ring, double position, bool finger, uint32_t timeMs) {
    LOGM(ERR, "FIXME: STUB: CTabletV2Protocol::ring not implemented");
}
