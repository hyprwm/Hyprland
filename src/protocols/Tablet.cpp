#include "Tablet.hpp"
#include "../devices/Tablet.hpp"
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
#include "core/Compositor.hpp"
#include <algorithm>
#include <cstring>

CTabletPadStripV2Resource::CTabletPadStripV2Resource(SP<CZwpTabletPadStripV2> resource_, uint32_t id_) : id(id_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpTabletPadStripV2* r) { PROTO::tablet->destroyResource(this); });
    resource->setOnDestroy([this](CZwpTabletPadStripV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletPadStripV2Resource::good() {
    return resource->resource();
}

CTabletPadRingV2Resource::CTabletPadRingV2Resource(SP<CZwpTabletPadRingV2> resource_, uint32_t id_) : id(id_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpTabletPadRingV2* r) { PROTO::tablet->destroyResource(this); });
    resource->setOnDestroy([this](CZwpTabletPadRingV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletPadRingV2Resource::good() {
    return resource->resource();
}

CTabletPadGroupV2Resource::CTabletPadGroupV2Resource(SP<CZwpTabletPadGroupV2> resource_, size_t idx_) : idx(idx_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpTabletPadGroupV2* r) { PROTO::tablet->destroyResource(this); });
    resource->setOnDestroy([this](CZwpTabletPadGroupV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletPadGroupV2Resource::good() {
    return resource->resource();
}

void CTabletPadGroupV2Resource::sendData(SP<CTabletPad> pad, SP<Aquamarine::ITabletPad::STabletPadGroup> group) {
    resource->sendModes(group->modes);

    wl_array buttonArr;
    wl_array_init(&buttonArr);
    wl_array_add(&buttonArr, group->buttons.size() * sizeof(int));
    memcpy(buttonArr.data, group->buttons.data(), group->buttons.size() * sizeof(int));
    resource->sendButtons(&buttonArr);
    wl_array_release(&buttonArr);

    for (size_t i = 0; i < group->strips.size(); ++i) {
        const auto RESOURCE =
            PROTO::tablet->m_vStrips.emplace_back(makeShared<CTabletPadStripV2Resource>(makeShared<CZwpTabletPadStripV2>(resource->client(), resource->version(), 0), i));

        if (!RESOURCE->good()) {
            resource->noMemory();
            PROTO::tablet->m_vStrips.pop_back();
            return;
        }

        resource->sendStrip(RESOURCE->resource.get());
    }

    for (size_t i = 0; i < group->rings.size(); ++i) {
        const auto RESOURCE =
            PROTO::tablet->m_vRings.emplace_back(makeShared<CTabletPadRingV2Resource>(makeShared<CZwpTabletPadRingV2>(resource->client(), resource->version(), 0), i));

        if (!RESOURCE->good()) {
            resource->noMemory();
            PROTO::tablet->m_vRings.pop_back();
            return;
        }

        resource->sendRing(RESOURCE->resource.get());
    }

    resource->sendDone();
}

CTabletPadV2Resource::CTabletPadV2Resource(SP<CZwpTabletPadV2> resource_, SP<CTabletPad> pad_, SP<CTabletSeat> seat_) : pad(pad_), seat(seat_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpTabletPadV2* r) { PROTO::tablet->destroyResource(this); });
    resource->setOnDestroy([this](CZwpTabletPadV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletPadV2Resource::good() {
    return resource->resource();
}

void CTabletPadV2Resource::sendData() {
    // this is dodgy as fuck. I hate wl_array. it's expanded wl_array_for_each because C++ would complain about the implicit casts
    for (auto& p : pad->aq()->paths) {
        resource->sendPath(p.c_str());
    }

    resource->sendButtons(pad->aq()->buttons);

    for (size_t i = 0; i < pad->aq()->groups.size(); ++i) {
        createGroup(pad->aq()->groups.at(i), i);
    }

    resource->sendDone();
}

void CTabletPadV2Resource::createGroup(SP<Aquamarine::ITabletPad::STabletPadGroup> group, size_t idx) {
    const auto RESOURCE =
        PROTO::tablet->m_vGroups.emplace_back(makeShared<CTabletPadGroupV2Resource>(makeShared<CZwpTabletPadGroupV2>(resource->client(), resource->version(), 0), idx));

    if (!RESOURCE->good()) {
        resource->noMemory();
        PROTO::tablet->m_vGroups.pop_back();
        return;
    }

    resource->sendGroup(RESOURCE->resource.get());

    RESOURCE->sendData(pad.lock(), group);
}

CTabletV2Resource::CTabletV2Resource(SP<CZwpTabletV2> resource_, SP<CTablet> tablet_, SP<CTabletSeat> seat_) : tablet(tablet_), seat(seat_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpTabletV2* r) { PROTO::tablet->destroyResource(this); });
    resource->setOnDestroy([this](CZwpTabletV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletV2Resource::good() {
    return resource->resource();
}

void CTabletV2Resource::sendData() {
    resource->sendName(tablet->deviceName.c_str());
    resource->sendId(tablet->aq()->usbVendorID, tablet->aq()->usbProductID);

    for (auto& p : tablet->aq()->paths) {
        resource->sendPath(p.c_str());
    }

    resource->sendDone();
}

CTabletToolV2Resource::CTabletToolV2Resource(SP<CZwpTabletToolV2> resource_, SP<CTabletTool> tool_, SP<CTabletSeat> seat_) : tool(tool_), seat(seat_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpTabletToolV2* r) { PROTO::tablet->destroyResource(this); });
    resource->setOnDestroy([this](CZwpTabletToolV2* r) { PROTO::tablet->destroyResource(this); });

    resource->setSetCursor([](CZwpTabletToolV2* r, uint32_t serial, wl_resource* surf, int32_t hot_x, int32_t hot_y) {
        if (!g_pSeatManager->state.pointerFocusResource || g_pSeatManager->state.pointerFocusResource->client() != r->client())
            return;

        g_pInputManager->processMouseRequest(CSeatManager::SSetCursorEvent{surf ? CWLSurfaceResource::fromResource(surf) : nullptr, {hot_x, hot_y}});
    });
}

CTabletToolV2Resource::~CTabletToolV2Resource() {
    if (frameSource)
        wl_event_source_remove(frameSource);
}

bool CTabletToolV2Resource::good() {
    return resource->resource();
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

    resource->sendType(AQ_TYPE_TO_PROTO(tool->aq()->type));
    resource->sendHardwareSerial(tool->aq()->serial >> 32, tool->aq()->serial & 0xFFFFFFFF);
    resource->sendHardwareIdWacom(tool->aq()->id >> 32, tool->aq()->id & 0xFFFFFFFF);
    if (tool->toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_DISTANCE)
        resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_DISTANCE);
    if (tool->toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_PRESSURE)
        resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE);
    if (tool->toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_ROTATION)
        resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_ROTATION);
    if (tool->toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_SLIDER)
        resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_SLIDER);
    if (tool->toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_TILT)
        resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_TILT);
    if (tool->toolCapabilities & CTabletTool::eTabletToolCapabilities::HID_TABLET_TOOL_CAPABILITY_WHEEL)
        resource->sendCapability(zwpTabletToolV2Capability::ZWP_TABLET_TOOL_V2_CAPABILITY_WHEEL);
    resource->sendDone();
}

void CTabletToolV2Resource::queueFrame() {
    if (frameSource)
        return;

    frameSource = wl_event_loop_add_idle(g_pCompositor->m_sWLEventLoop, [](void* data) { ((CTabletToolV2Resource*)data)->sendFrame(false); }, this);
}

void CTabletToolV2Resource::sendFrame(bool removeSource) {
    if (frameSource) {
        if (removeSource)
            wl_event_source_remove(frameSource);
        frameSource = nullptr;
    }

    if (!current)
        return;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    resource->sendFrame(now.tv_sec * 1000 + now.tv_nsec / 1000000);
}

CTabletSeat::CTabletSeat(SP<CZwpTabletSeatV2> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpTabletSeatV2* r) { PROTO::tablet->destroyResource(this); });
    resource->setOnDestroy([this](CZwpTabletSeatV2* r) { PROTO::tablet->destroyResource(this); });
}

bool CTabletSeat::good() {
    return resource->resource();
}

void CTabletSeat::sendTool(SP<CTabletTool> tool) {
    const auto RESOURCE =
        PROTO::tablet->m_vTools.emplace_back(makeShared<CTabletToolV2Resource>(makeShared<CZwpTabletToolV2>(resource->client(), resource->version(), 0), tool, self.lock()));

    if (!RESOURCE->good()) {
        resource->noMemory();
        PROTO::tablet->m_vTools.pop_back();
        return;
    }

    resource->sendToolAdded(RESOURCE->resource.get());

    RESOURCE->sendData();
    tools.push_back(RESOURCE);
}

void CTabletSeat::sendPad(SP<CTabletPad> pad) {
    const auto RESOURCE =
        PROTO::tablet->m_vPads.emplace_back(makeShared<CTabletPadV2Resource>(makeShared<CZwpTabletPadV2>(resource->client(), resource->version(), 0), pad, self.lock()));

    if (!RESOURCE->good()) {
        resource->noMemory();
        PROTO::tablet->m_vPads.pop_back();
        return;
    }

    resource->sendPadAdded(RESOURCE->resource.get());

    RESOURCE->sendData();
    pads.push_back(RESOURCE);
}

void CTabletSeat::sendTablet(SP<CTablet> tablet) {
    const auto RESOURCE =
        PROTO::tablet->m_vTablets.emplace_back(makeShared<CTabletV2Resource>(makeShared<CZwpTabletV2>(resource->client(), resource->version(), 0), tablet, self.lock()));

    if (!RESOURCE->good()) {
        resource->noMemory();
        PROTO::tablet->m_vTablets.pop_back();
        return;
    }

    resource->sendTabletAdded(RESOURCE->resource.get());

    RESOURCE->sendData();
    tablets.push_back(RESOURCE);
}

void CTabletSeat::sendData() {
    for (auto& tw : PROTO::tablet->tablets) {
        if (tw.expired())
            continue;

        sendTablet(tw.lock());
    }

    for (auto& tw : PROTO::tablet->tools) {
        if (tw.expired())
            continue;

        sendTool(tw.lock());
    }

    for (auto& tw : PROTO::tablet->pads) {
        if (tw.expired())
            continue;

        sendPad(tw.lock());
    }
}

CTabletV2Protocol::CTabletV2Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CTabletV2Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpTabletManagerV2>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpTabletManagerV2* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpTabletManagerV2* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetTabletSeat([this](CZwpTabletManagerV2* pMgr, uint32_t id, wl_resource* seat) { this->onGetSeat(pMgr, id, seat); });
}

void CTabletV2Protocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CTabletV2Protocol::destroyResource(CTabletSeat* resource) {
    std::erase_if(m_vSeats, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletToolV2Resource* resource) {
    std::erase_if(m_vTools, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletV2Resource* resource) {
    std::erase_if(m_vTablets, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletPadV2Resource* resource) {
    std::erase_if(m_vPads, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletPadGroupV2Resource* resource) {
    std::erase_if(m_vGroups, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletPadRingV2Resource* resource) {
    std::erase_if(m_vRings, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::destroyResource(CTabletPadStripV2Resource* resource) {
    std::erase_if(m_vStrips, [&](const auto& other) { return other.get() == resource; });
}

void CTabletV2Protocol::onGetSeat(CZwpTabletManagerV2* pMgr, uint32_t id, wl_resource* seat) {
    const auto RESOURCE = m_vSeats.emplace_back(makeShared<CTabletSeat>(makeShared<CZwpTabletSeatV2>(pMgr->client(), pMgr->version(), id)));

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vSeats.pop_back();
        return;
    }

    RESOURCE->self = RESOURCE;
    RESOURCE->sendData();
}

void CTabletV2Protocol::registerDevice(SP<CTablet> tablet) {
    for (auto& s : m_vSeats) {
        s->sendTablet(tablet);
    }

    tablets.push_back(tablet);
}

void CTabletV2Protocol::registerDevice(SP<CTabletTool> tool) {
    for (auto& s : m_vSeats) {
        s->sendTool(tool);
    }

    tools.push_back(tool);
}

void CTabletV2Protocol::registerDevice(SP<CTabletPad> pad) {
    for (auto& s : m_vSeats) {
        s->sendPad(pad);
    }

    pads.push_back(pad);
}

void CTabletV2Protocol::unregisterDevice(SP<CTablet> tablet) {
    for (auto& t : m_vTablets) {
        if (t->tablet == tablet) {
            t->resource->sendRemoved();
            t->inert = true;
        }
    }
    std::erase_if(tablets, [tablet](const auto& e) { return e.expired() || e == tablet; });
}

void CTabletV2Protocol::unregisterDevice(SP<CTabletTool> tool) {
    for (auto& t : m_vTools) {
        if (t->tool == tool) {
            t->resource->sendRemoved();
            t->inert = true;
        }
    }
    std::erase_if(tools, [tool](const auto& e) { return e.expired() || e == tool; });
}

void CTabletV2Protocol::unregisterDevice(SP<CTabletPad> pad) {
    for (auto& t : m_vPads) {
        if (t->pad == pad) {
            t->resource->sendRemoved();
            t->inert = true;
        }
    }
    std::erase_if(pads, [pad](const auto& e) { return e.expired() || e == pad; });
}

void CTabletV2Protocol::recheckRegisteredDevices() {
    std::erase_if(tablets, [](const auto& e) { return e.expired(); });
    std::erase_if(tools, [](const auto& e) { return e.expired(); });
    std::erase_if(pads, [](const auto& e) { return e.expired(); });

    // now we need to send removed events
    for (auto& t : m_vTablets) {
        if (!t->tablet.expired() || t->inert)
            continue;

        t->resource->sendRemoved();
        t->inert = true;
    }

    for (auto& t : m_vTools) {
        if (!t->tool.expired() || t->inert)
            continue;

        if (t->current) {
            t->resource->sendProximityOut();
            t->sendFrame();
            t->lastSurf.reset();
        }

        t->resource->sendRemoved();
        t->inert = true;
    }

    for (auto& t : m_vPads) {
        if (!t->pad.expired() || t->inert)
            continue;

        t->resource->sendRemoved();
        t->inert = true;
    }
}

void CTabletV2Protocol::pressure(SP<CTabletTool> tool, double value) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->resource->sendPressure(std::clamp(value * 65535, 0.0, 65535.0));
        t->queueFrame();
    }
}

void CTabletV2Protocol::distance(SP<CTabletTool> tool, double value) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->resource->sendDistance(std::clamp(value * 65535, 0.0, 65535.0));
        t->queueFrame();
    }
}

void CTabletV2Protocol::rotation(SP<CTabletTool> tool, double value) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->resource->sendRotation(wl_fixed_from_double(value));
        t->queueFrame();
    }
}

void CTabletV2Protocol::slider(SP<CTabletTool> tool, double value) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->resource->sendSlider(std::clamp(value * 65535, -65535.0, 65535.0));
        t->queueFrame();
    }
}

void CTabletV2Protocol::wheel(SP<CTabletTool> tool, double value) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->resource->sendWheel(wl_fixed_from_double(value), 0);
        t->queueFrame();
    }
}

void CTabletV2Protocol::tilt(SP<CTabletTool> tool, const Vector2D& value) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->resource->sendTilt(wl_fixed_from_double(value.x), wl_fixed_from_double(value.y));
        t->queueFrame();
    }
}

void CTabletV2Protocol::up(SP<CTabletTool> tool) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->resource->sendUp();
        t->queueFrame();
    }
}

void CTabletV2Protocol::down(SP<CTabletTool> tool) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        auto serial = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(t->resource->client()));
        t->resource->sendDown(serial);
        t->queueFrame();
    }
}

void CTabletV2Protocol::proximityIn(SP<CTabletTool> tool, SP<CTablet> tablet, SP<CWLSurfaceResource> surf) {
    proximityOut(tool);
    const auto                CLIENT = surf->client();

    SP<CTabletToolV2Resource> toolResource;
    SP<CTabletV2Resource>     tabletResource;

    for (auto& t : m_vTools) {
        if (t->tool != tool || t->resource->client() != CLIENT)
            continue;

        if (t->seat.expired()) {
            LOGM(ERR, "proximityIn on a tool without a seat parent");
            return;
        }

        if (t->lastSurf == surf)
            return;

        toolResource = t;

        for (auto& tab : m_vTablets) {
            if (tab->tablet != tablet)
                continue;

            if (tab->seat != t->seat || !tab->seat)
                continue;

            tabletResource = tab;
            break;
        }
    }

    if (!tabletResource || !toolResource) {
        LOGM(ERR, "proximityIn on a tool and tablet without valid resource(s)??");
        return;
    }

    toolResource->current  = true;
    toolResource->lastSurf = surf;

    auto serial = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(toolResource->resource->client()));
    toolResource->resource->sendProximityIn(serial, tabletResource->resource.get(), surf->getResource()->resource());
    toolResource->queueFrame();

    LOGM(ERR, "proximityIn: found no resource to send enter");
}

void CTabletV2Protocol::proximityOut(SP<CTabletTool> tool) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->lastSurf.reset();
        t->resource->sendProximityOut();
        t->sendFrame();
        t->current = false;
    }
}

void CTabletV2Protocol::buttonTool(SP<CTabletTool> tool, uint32_t button, uint32_t state) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        auto serial = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(t->resource->client()));
        t->resource->sendButton(serial, button, (zwpTabletToolV2ButtonState)state);
        t->queueFrame();
    }
}

void CTabletV2Protocol::motion(SP<CTabletTool> tool, const Vector2D& value) {
    for (auto& t : m_vTools) {
        if (t->tool != tool || !t->current)
            continue;

        t->resource->sendMotion(wl_fixed_from_double(value.x), wl_fixed_from_double(value.y));
        t->queueFrame();
    }
}

void CTabletV2Protocol::mode(SP<CTabletPad> pad, uint32_t group, uint32_t mode, uint32_t timeMs) {
    for (auto& t : m_vPads) {
        if (t->pad != pad)
            continue;
        if (t->groups.size() <= group) {
            LOGM(ERR, "BUG THIS: group >= t->groups.size()");
            return;
        }
        auto serial = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(t->resource->client()));
        t->groups.at(group)->resource->sendModeSwitch(timeMs, serial, mode);
    }
}

void CTabletV2Protocol::buttonPad(SP<CTabletPad> pad, uint32_t button, uint32_t timeMs, uint32_t state) {
    for (auto& t : m_vPads) {
        if (t->pad != pad)
            continue;
        t->resource->sendButton(timeMs, button, zwpTabletToolV2ButtonState{state});
    }
}

void CTabletV2Protocol::strip(SP<CTabletPad> pad, uint32_t strip, double position, bool finger, uint32_t timeMs) {
    LOGM(ERR, "FIXME: STUB: CTabletV2Protocol::strip not implemented");
}

void CTabletV2Protocol::ring(SP<CTabletPad> pad, uint32_t ring, double position, bool finger, uint32_t timeMs) {
    LOGM(ERR, "FIXME: STUB: CTabletV2Protocol::ring not implemented");
}
