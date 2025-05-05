#include "PointerGestures.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
#include "core/Compositor.hpp"

CPointerGestureSwipe::CPointerGestureSwipe(SP<CZwpPointerGestureSwipeV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setOnDestroy([this](CZwpPointerGestureSwipeV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
    m_resource->setDestroy([this](CZwpPointerGestureSwipeV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
}

bool CPointerGestureSwipe::good() {
    return m_resource->resource();
}

CPointerGestureHold::CPointerGestureHold(SP<CZwpPointerGestureHoldV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setOnDestroy([this](CZwpPointerGestureHoldV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
    m_resource->setDestroy([this](CZwpPointerGestureHoldV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
}

bool CPointerGestureHold::good() {
    return m_resource->resource();
}

CPointerGesturePinch::CPointerGesturePinch(SP<CZwpPointerGesturePinchV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setOnDestroy([this](CZwpPointerGesturePinchV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
    m_resource->setDestroy([this](CZwpPointerGesturePinchV1* p) { PROTO::pointerGestures->onGestureDestroy(this); });
}

bool CPointerGesturePinch::good() {
    return m_resource->resource();
}

CPointerGesturesProtocol::CPointerGesturesProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CPointerGesturesProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwpPointerGesturesV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpPointerGesturesV1* p) { this->onManagerResourceDestroy(p->resource()); });
    RESOURCE->setRelease([this](CZwpPointerGesturesV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });

    RESOURCE->setGetHoldGesture([this](CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) { this->onGetHoldGesture(pMgr, id, pointer); });
    RESOURCE->setGetPinchGesture([this](CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) { this->onGetPinchGesture(pMgr, id, pointer); });
    RESOURCE->setGetSwipeGesture([this](CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) { this->onGetSwipeGesture(pMgr, id, pointer); });
}

void CPointerGesturesProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CPointerGesturesProtocol::onGestureDestroy(CPointerGestureSwipe* gesture) {
    std::erase_if(m_swipes, [&](const auto& other) { return other.get() == gesture; });
}

void CPointerGesturesProtocol::onGestureDestroy(CPointerGesturePinch* gesture) {
    std::erase_if(m_pinches, [&](const auto& other) { return other.get() == gesture; });
}

void CPointerGesturesProtocol::onGestureDestroy(CPointerGestureHold* gesture) {
    std::erase_if(m_holds, [&](const auto& other) { return other.get() == gesture; });
}

void CPointerGesturesProtocol::onGetPinchGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_pinches.emplace_back(makeUnique<CPointerGesturePinch>(makeShared<CZwpPointerGesturePinchV1>(CLIENT, pMgr->version(), id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        LOGM(ERR, "Couldn't create gesture");
        return;
    }
}

void CPointerGesturesProtocol::onGetSwipeGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_swipes.emplace_back(makeUnique<CPointerGestureSwipe>(makeShared<CZwpPointerGestureSwipeV1>(CLIENT, pMgr->version(), id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        LOGM(ERR, "Couldn't create gesture");
        return;
    }
}

void CPointerGesturesProtocol::onGetHoldGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_holds.emplace_back(makeUnique<CPointerGestureHold>(makeShared<CZwpPointerGestureHoldV1>(CLIENT, pMgr->version(), id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        LOGM(ERR, "Couldn't create gesture");
        return;
    }
}

void CPointerGesturesProtocol::swipeBegin(uint32_t timeMs, uint32_t fingers) {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->m_state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->m_state.pointerFocusResource.lock());

    for (auto const& sw : m_swipes) {
        if (sw->m_resource->client() != FOCUSEDCLIENT)
            continue;

        sw->m_resource->sendBegin(SERIAL, timeMs, g_pSeatManager->m_state.pointerFocus->getResource()->resource(), fingers);
    }
}

void CPointerGesturesProtocol::swipeUpdate(uint32_t timeMs, const Vector2D& delta) {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->m_state.pointerFocusResource->client();

    for (auto const& sw : m_swipes) {
        if (sw->m_resource->client() != FOCUSEDCLIENT)
            continue;

        sw->m_resource->sendUpdate(timeMs, wl_fixed_from_double(delta.x), wl_fixed_from_double(delta.y));
    }
}

void CPointerGesturesProtocol::swipeEnd(uint32_t timeMs, bool cancelled) {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->m_state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->m_state.pointerFocusResource.lock());

    for (auto const& sw : m_swipes) {
        if (sw->m_resource->client() != FOCUSEDCLIENT)
            continue;

        sw->m_resource->sendEnd(SERIAL, timeMs, cancelled);
    }
}

void CPointerGesturesProtocol::pinchBegin(uint32_t timeMs, uint32_t fingers) {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->m_state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->m_state.pointerFocusResource.lock());

    for (auto const& sw : m_pinches) {
        if (sw->m_resource->client() != FOCUSEDCLIENT)
            continue;

        sw->m_resource->sendBegin(SERIAL, timeMs, g_pSeatManager->m_state.pointerFocus->getResource()->resource(), fingers);
    }
}

void CPointerGesturesProtocol::pinchUpdate(uint32_t timeMs, const Vector2D& delta, double scale, double rotation) {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->m_state.pointerFocusResource->client();

    for (auto const& sw : m_pinches) {
        if (sw->m_resource->client() != FOCUSEDCLIENT)
            continue;

        sw->m_resource->sendUpdate(timeMs, wl_fixed_from_double(delta.x), wl_fixed_from_double(delta.y), wl_fixed_from_double(scale), wl_fixed_from_double(rotation));
    }
}

void CPointerGesturesProtocol::pinchEnd(uint32_t timeMs, bool cancelled) {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->m_state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->m_state.pointerFocusResource.lock());

    for (auto const& sw : m_pinches) {
        if (sw->m_resource->client() != FOCUSEDCLIENT)
            continue;

        sw->m_resource->sendEnd(SERIAL, timeMs, cancelled);
    }
}

void CPointerGesturesProtocol::holdBegin(uint32_t timeMs, uint32_t fingers) {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->m_state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->m_state.pointerFocusResource.lock());

    for (auto const& sw : m_holds) {
        if (sw->m_resource->client() != FOCUSEDCLIENT)
            continue;

        sw->m_resource->sendBegin(SERIAL, timeMs, g_pSeatManager->m_state.pointerFocus->getResource()->resource(), fingers);
    }
}

void CPointerGesturesProtocol::holdEnd(uint32_t timeMs, bool cancelled) {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    const auto FOCUSEDCLIENT = g_pSeatManager->m_state.pointerFocusResource->client();

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->m_state.pointerFocusResource.lock());

    for (auto const& sw : m_holds) {
        if (sw->m_resource->client() != FOCUSEDCLIENT)
            continue;

        sw->m_resource->sendEnd(SERIAL, timeMs, cancelled);
    }
}
