#include "PointerGestures.hpp"
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
#include "core/Compositor.hpp"

CPointerGestureSwipe::CPointerGestureSwipe(SP<CZwpPointerGestureSwipeV1> resource_) : resource(resource_) {
    if (!resource->resource())
        return;

    resource->setOnDestroy([this](CZwpPointerGestureSwipeV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
    resource->setDestroy([this](CZwpPointerGestureSwipeV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
}

bool CPointerGestureSwipe::good() {
    return resource->resource();
}

CPointerGestureHold::CPointerGestureHold(SP<CZwpPointerGestureHoldV1> resource_) : resource(resource_) {
    if (!resource->resource())
        return;

    resource->setOnDestroy([this](CZwpPointerGestureHoldV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
    resource->setDestroy([this](CZwpPointerGestureHoldV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
}

bool CPointerGestureHold::good() {
    return resource->resource();
}

CPointerGesturePinch::CPointerGesturePinch(SP<CZwpPointerGesturePinchV1> resource_) : resource(resource_) {
    if (!resource->resource())
        return;

    resource->setOnDestroy([this](CZwpPointerGesturePinchV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
    resource->setDestroy([this](CZwpPointerGesturePinchV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
}

bool CPointerGesturePinch::good() {
    return resource->resource();
}

CPointerGesturesProtocol::CPointerGesturesProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CPointerGesturesProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpPointerGesturesV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpPointerGesturesV1* p) { this->onManagerResourceDestroy(p->resource()); });
    RESOURCE->setRelease([this](CZwpPointerGesturesV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });

    RESOURCE->setGetHoldGesture([this](CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) { this->onGetHoldGesture(pMgr, id, pointer); });
    RESOURCE->setGetPinchGesture([this](CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) { this->onGetPinchGesture(pMgr, id, pointer); });
    RESOURCE->setGetSwipeGesture([this](CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) { this->onGetSwipeGesture(pMgr, id, pointer); });
}

void CPointerGesturesProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CPointerGesturesProtocol::onGestureDestroy(CPointerGestureSwipe* gesture) {
    std::erase_if(m_vSwipes, [&](const auto& other) { return other.get() == gesture; });
}

void CPointerGesturesProtocol::onGestureDestroy(CPointerGesturePinch* gesture) {
    std::erase_if(m_vPinches, [&](const auto& other) { return other.get() == gesture; });
}

void CPointerGesturesProtocol::onGestureDestroy(CPointerGestureHold* gesture) {
    std::erase_if(m_vHolds, [&](const auto& other) { return other.get() == gesture; });
}

void CPointerGesturesProtocol::onGetPinchGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vPinches.emplace_back(std::make_unique<CPointerGesturePinch>(makeShared<CZwpPointerGesturePinchV1>(CLIENT, pMgr->version(), id))).get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        LOGM(ERR, "Couldn't create gesture");
        return;
    }
}

void CPointerGesturesProtocol::onGetSwipeGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vSwipes.emplace_back(std::make_unique<CPointerGestureSwipe>(makeShared<CZwpPointerGestureSwipeV1>(CLIENT, pMgr->version(), id))).get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        LOGM(ERR, "Couldn't create gesture");
        return;
    }
}

void CPointerGesturesProtocol::onGetHoldGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vHolds.emplace_back(std::make_unique<CPointerGestureHold>(makeShared<CZwpPointerGestureHoldV1>(CLIENT, pMgr->version(), id))).get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        LOGM(ERR, "Couldn't create gesture");
        return;
    }
}

void CPointerGesturesProtocol::swipeBegin(uint32_t timeMs, uint32_t fingers) {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->state.pointerFocusResource.lock());

    for (auto& sw : m_vSwipes) {
        if (sw->resource->client() != FOCUSEDCLIENT)
            continue;

        sw->resource->sendBegin(SERIAL, timeMs, g_pSeatManager->state.pointerFocus->getResource()->resource(), fingers);
    }
}

void CPointerGesturesProtocol::swipeUpdate(uint32_t timeMs, const Vector2D& delta) {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->state.pointerFocusResource->client();

    for (auto& sw : m_vSwipes) {
        if (sw->resource->client() != FOCUSEDCLIENT)
            continue;

        sw->resource->sendUpdate(timeMs, wl_fixed_from_double(delta.x), wl_fixed_from_double(delta.y));
    }
}

void CPointerGesturesProtocol::swipeEnd(uint32_t timeMs, bool cancelled) {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->state.pointerFocusResource.lock());

    for (auto& sw : m_vSwipes) {
        if (sw->resource->client() != FOCUSEDCLIENT)
            continue;

        sw->resource->sendEnd(SERIAL, timeMs, cancelled);
    }
}

void CPointerGesturesProtocol::pinchBegin(uint32_t timeMs, uint32_t fingers) {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->state.pointerFocusResource.lock());

    for (auto& sw : m_vPinches) {
        if (sw->resource->client() != FOCUSEDCLIENT)
            continue;

        sw->resource->sendBegin(SERIAL, timeMs, g_pSeatManager->state.pointerFocus->getResource()->resource(), fingers);
    }
}

void CPointerGesturesProtocol::pinchUpdate(uint32_t timeMs, const Vector2D& delta, double scale, double rotation) {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->state.pointerFocusResource->client();

    for (auto& sw : m_vPinches) {
        if (sw->resource->client() != FOCUSEDCLIENT)
            continue;

        sw->resource->sendUpdate(timeMs, wl_fixed_from_double(delta.x), wl_fixed_from_double(delta.y), wl_fixed_from_double(scale), wl_fixed_from_double(rotation));
    }
}

void CPointerGesturesProtocol::pinchEnd(uint32_t timeMs, bool cancelled) {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->state.pointerFocusResource.lock());

    for (auto& sw : m_vPinches) {
        if (sw->resource->client() != FOCUSEDCLIENT)
            continue;

        sw->resource->sendEnd(SERIAL, timeMs, cancelled);
    }
}

void CPointerGesturesProtocol::holdBegin(uint32_t timeMs, uint32_t fingers) {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->state.pointerFocusResource.lock());

    for (auto& sw : m_vHolds) {
        if (sw->resource->client() != FOCUSEDCLIENT)
            continue;

        sw->resource->sendBegin(SERIAL, timeMs, g_pSeatManager->state.pointerFocus->getResource()->resource(), fingers);
    }
}

void CPointerGesturesProtocol::holdEnd(uint32_t timeMs, bool cancelled) {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->state.pointerFocusResource.lock());

    for (auto& sw : m_vHolds) {
        if (sw->resource->client() != FOCUSEDCLIENT)
            continue;

        sw->resource->sendEnd(SERIAL, timeMs, cancelled);
    }
}
