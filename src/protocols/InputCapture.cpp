#include "InputCapture.hpp"

#include "Compositor.hpp"
#include "debug/Log.hpp"
#include "helpers/Eis.hpp"
#include "hyprland-input-capture-v1.hpp"
#include "managers/HookSystemManager.hpp"
#include "protocols/WaylandProtocol.hpp"
#include "render/Renderer.hpp"
#include <algorithm>
#include <cairo.h>
#include <cstdint>
#include <fcntl.h>
#include <glaze/core/context.hpp>
#include <glaze/util/parse.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <libeis.h>
#include <optional>
#include <string>

static int eisCounter = 0;

CInputCaptureResource::CInputCaptureResource(SP<CHyprlandInputCaptureV1> resource_, std::string handle) : sessionId(handle), m_resource(resource_) {
    if UNLIKELY (!good())
        return;
    Debug::log(LOG, "[input-capture]({}) new session", sessionId.c_str());

    m_resource->setOnDestroy([this](CHyprlandInputCaptureV1* r) { PROTO::inputCapture->destroyResource(this); }); //Remove & free this session

    m_resource->setEnable([this](CHyprlandInputCaptureV1* r) { onEnable(); });
    m_resource->setAddBarrier(
        [this](CHyprlandInputCaptureV1* r, uint32_t zoneSet, uint32_t id, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) { onAddBarrier(zoneSet, id, x1, y1, x2, y2); });
    m_resource->setDisable([this](CHyprlandInputCaptureV1* r) { disable(); });
    m_resource->setRelease([this](CHyprlandInputCaptureV1* r, uint32_t activationId, double x, double y) { onRelease(activationId, x, y); });
    m_resource->setClearBarriers([this](CHyprlandInputCaptureV1* r) { onClearBarriers(); });

    eis = makeUnique<CEmulatedInputServer>("eis-" + std::to_string(eisCounter++));

    m_resource->sendEisFd(eis->getFileDescriptor());

    monitorCallback = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) {
        disable();
        eis->resetPointer();
    });
}

CInputCaptureResource::~CInputCaptureResource() {
    if (status == ACTIVATED)
        PROTO::inputCapture->forceRelease();

    Debug::log(LOG, "[input-capture]({}) session destroyed", sessionId.c_str());
    g_pHookSystem->unhook(monitorCallback);
};

bool CInputCaptureResource::good() {
    return m_resource->resource();
}

void CInputCaptureResource::onEnable() {
    status = ENABLED;
}

enum eValidResult : uint8_t {
    INVALID,
    PARTIAL,
    VALID
};

static eValidResult isBarrierValidAgainstMonitor(int x1, int y1, int x2, int y2, PHLMONITOR monitor) {
    int mx1 = monitor->m_position.x;
    int my1 = monitor->m_position.y;
    int mx2 = mx1 + monitor->m_pixelSize.x - 1;
    int my2 = my1 + monitor->m_pixelSize.y - 1;
    Debug::log(LOG, "[input-capture] mx1: {} my1: {} mx2: {} mx2: {}", mx1, my1, mx2, my2);

    if (x1 == x2) {                     //If zone is vertical
        if (x1 != mx1 && x1 != mx2 + 1) //If the zone don't touch the left or right side
            return INVALID;

        if (y1 != my1 || y2 != my2) {                                 //If the zone is shorter than the height of the screen
            if ((my1 <= y1 && y1 <= my2) || (my1 <= y2 && y2 <= my2)) //Maybe the segments are overlapping
                return PARTIAL;
            return INVALID;
        }
    } else {
        if (y1 != my1 && y1 != my2 + 1) //If the zone don't touch the bottom or top side
            return INVALID;

        if (x1 != mx1 || x2 != mx2) {                                 //If the zone is shorter than the height of the screen
            if ((mx1 <= x1 && x1 <= mx2) || (mx1 <= x2 && x2 <= mx2)) //Maybe the segments are overlapping
                return PARTIAL;
            return INVALID;
        }
    }

    return VALID;
}

static bool isBarrierValid(int x1, int y1, int x2, int y2) {
    if (x1 != x2 && y1 != y2) //At least one axis should be aligned
        return false;

    if (x1 == x2 && y1 == y2) //The barrier should have non-null area
        return false;

    if (x1 > x2)
        std::swap(x1, x2);

    if (y1 > y2)
        std::swap(y1, y2);

    int valid   = 0;
    int partial = 0; //Used to detect if a barrier is placed on the side of two monitors
    for (auto& o : g_pCompositor->m_monitors) {
        switch (isBarrierValidAgainstMonitor(x1, y1, x2, y2, o)) {
            case VALID: valid++; break;
            case PARTIAL: partial++; break;
            case INVALID: break;
        }
    }

    return valid == 1 && partial == 0;
}

void CInputCaptureResource::onAddBarrier(uint32_t zoneSet, uint32_t id, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) {
    bool valid = isBarrierValid(x1, y1, x2, y2);

    if (!valid) {
        m_resource->error(HYPRLAND_INPUT_CAPTURE_V1_ERROR_INVALID_BARRIER, "The barrier id " + std::to_string(id) + " is invalid");
        Debug::log(LOG, "[input-capture]({}) Barrier {} is invalid [{}, {}], [{}, {}]", sessionId.c_str(), id, x1, y1, x2, y2);

        return;
    }

    Debug::log(LOG, "[input-capture]({}) Barrier {} [{}, {}], [{}, {}] added", sessionId.c_str(), id, x1, y1, x2, y2);

    PROTO::inputCapture->addBarrier({.sessionId = sessionId, .id = id, .x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2});
}

void CInputCaptureResource::onDisable() {
    disable();
}

void CInputCaptureResource::onRelease(uint32_t activationId_, double x, double y) {
    if (activationId_ != activationId) { // If id is invalid we still want to release the mouse to avoid any issue
        Debug::log(WARN, "[input-capture]({}) Invalid activation id {} expected {}", sessionId.c_str(), activationId_, activationId);
    }

    deactivate();

    if (x != -1 && y != -1)
        g_pCompositor->warpCursorTo({x, y}, true);
    g_pHyprRenderer->ensureCursorRenderingMode();
}

void CInputCaptureResource::onClearBarriers() {
    PROTO::inputCapture->clearBarriers(sessionId);
}

void CInputCaptureResource::activate(double x, double y, uint32_t borderId) {
    if (status != ENABLED)
        return;

    activationId += 5;
    status = ACTIVATED;
    Debug::log(LOG, "[input-capture]({}) Input captured, activationId: {}, borderId: {}, x: {}, y: {}", sessionId.c_str(), activationId, borderId, x, y);
    eis->startEmulating(activationId);
    g_pHyprRenderer->ensureCursorRenderingMode();

    m_resource->sendActivated(activationId, x, y, borderId);
}

void CInputCaptureResource::deactivate() {
    if (status != ACTIVATED)
        return;

    Debug::log(LOG, "[input-capture]({}) Input released", sessionId.c_str());
    status = ENABLED;
    eis->stopEmulating();
    PROTO::inputCapture->release();

    m_resource->sendDeactivated(activationId);
}

void CInputCaptureResource::disable() {
    if (status == ACTIVATED)
        deactivate();
    if (status != ENABLED)
        return;
    status = CREATED;
    PROTO::inputCapture->clearBarriers(sessionId);

    m_resource->sendDisabled();
}

void CInputCaptureResource::motion(double dx, double dy) {
    eis->sendMotion(dx, dy);
}

void CInputCaptureResource::key(uint32_t key, bool pressed) {
    eis->sendKey(key, pressed);
}

void CInputCaptureResource::modifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group) {
    eis->sendModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void CInputCaptureResource::button(uint32_t button, bool pressed) {
    eis->sendButton(button, pressed);
}

void CInputCaptureResource::axis(bool axis, double value) {
    int32_t x = 0;
    int32_t y = 0;

    if (axis)
        x = value;
    else
        y = value;

    eis->sendScrollDelta(x, y);
}

void CInputCaptureResource::axisValue120(bool axis, int32_t value120) {
    int32_t x = 0;
    int32_t y = 0;

    if (axis)
        x = value120;
    else
        y = value120;

    eis->sendScrollDiscrete(x, y);
}

void CInputCaptureResource::axisStop(bool axis) {
    eis->sendScrollStop(axis, !axis);
}

void CInputCaptureResource::frame() {
    eis->sendPointerFrame();
}

static bool isLineIntersect(double p0X, double p0Y, double p1X, double p1Y, double p2X, double p2Y, double p3X, double p3Y) {
    float s1X = p1X - p0X;
    float s1Y = p1Y - p0Y;
    float s2X = p3X - p2X;
    float s2Y = p3Y - p2Y;
    float s   = (-s1Y * (p0X - p2X) + s1X * (p0Y - p2Y)) / (-s2X * s1Y + s1X * s2Y);
    float t   = (s2X * (p0Y - p2Y) - s2Y * (p0X - p2X)) / (-s2X * s1Y + s1X * s2Y);

    return s >= 0 && s <= 1 && t >= 0 && t <= 1;
}

static bool testCollision(SBarrier barrier, double px, double py, double nx, double ny) {
    return isLineIntersect(barrier.x1, barrier.y1, barrier.x2, barrier.y2, px, py, nx, ny);
}

std::optional<SBarrier> CInputCaptureProtocol::isColliding(double px, double py, double nx, double ny) {
    for (const auto& barrier : barriers)
        if (testCollision(barrier, px, py, nx, ny))
            return std::optional(barrier);

    return std::nullopt;
}

CInputCaptureProtocol::CInputCaptureProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CInputCaptureProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto& RESOURCE = m_vManagers.emplace_back(makeUnique<CHyprlandInputCaptureManagerV1>(client, ver, id));

    RESOURCE->setOnDestroy([this](CHyprlandInputCaptureManagerV1* p) { std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == p->resource(); }); });

    RESOURCE->setCreateSession([this](CHyprlandInputCaptureManagerV1* pMgr, uint32_t id, std::string handle) { onCreateSession(pMgr, id, handle); });
}

void CInputCaptureProtocol::onCreateSession(CHyprlandInputCaptureManagerV1* pMgr, uint32_t id, std::string handle) {
    const auto RESOURCE = m_Sessions.emplace_back(makeShared<CInputCaptureResource>(makeShared<CHyprlandInputCaptureV1>(pMgr->client(), pMgr->version(), id), handle));

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_Sessions.pop_back();
        return;
    }

    LOGM(LOG, "New InputCapture at id {}", id);
}

void CInputCaptureProtocol::destroyResource(CInputCaptureResource* resource) {
    Debug::log(LOG, "Destroying resource {}", resource->sessionId);
    clearBarriers(resource->sessionId);
    std::erase_if(m_Sessions, [resource](const auto& other) {
        if (other->sessionId == resource->sessionId)
            Debug::log(LOG, "FIND {}", other->sessionId);
        return other->sessionId == resource->sessionId;
    });
}

bool CInputCaptureProtocol::isCaptured() {
    return active != nullptr;
}

void CInputCaptureProtocol::updateKeymap() {
    for (auto& session : m_Sessions)
        session->updateKeymap();
}

void CInputCaptureProtocol::motion(const Vector2D& absolutePosition, const Vector2D& delta) {
    auto [x, y]   = absolutePosition;
    auto [dx, dy] = delta;

    auto matched = isColliding(x, y, x - dx, y - dy);
    if (matched.has_value()) {
        auto result = std::ranges::find_if(m_Sessions, [matched](SP<CInputCaptureResource> elem) { return matched.value().sessionId == elem->sessionId; });
        if (result == m_Sessions.end()) {
            LOGM(ERR, "Cannot find session {} for triggering barrier {}", matched.value().id, matched.value().sessionId);
            return;
        }
        (*result)->activate(x, y, matched.value().id);
        active = *result;
    }

    if (active)
        active->motion(dx, dy);
}

void CInputCaptureProtocol::addBarrier(SBarrier barrier) {
    barriers.emplace_back(barrier);
}

void CInputCaptureProtocol::clearBarriers(std::string sessionId) {
    std::erase_if(barriers, [sessionId](SBarrier b) { return b.sessionId == sessionId; });
    Debug::log(LOG, "[input-capture]({}) Barriers cleared", sessionId.c_str());
}

void CInputCaptureProtocol::release() {
    active = nullptr;
}

void CInputCaptureProtocol::forceRelease() {
    Debug::log(LOG, "[input-capture] Force release input capture");
    if (active)
        active->disable();
    release();
}

void CInputCaptureProtocol::key(uint32_t keyCode, wl_keyboard_key_state state) {
    if (active)
        active->key(keyCode, state);
}

void CInputCaptureProtocol::modifiers(uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    if (active)
        active->modifiers(mods_depressed, mods_locked, mods_locked, group);
}

void CInputCaptureProtocol::button(uint32_t button, wl_pointer_button_state state) {
    if (active)
        active->button(button, state);
}

void CInputCaptureProtocol::axis(wl_pointer_axis axis, double value) {
    if (active)
        active->axis(axis, value);
}

void CInputCaptureProtocol::axisValue120(wl_pointer_axis axis, int32_t value120) {
    if (active)
        active->axisValue120(axis, value120);
}

void CInputCaptureProtocol::axisStop(wl_pointer_axis axis) {
    if (active)
        active->axisStop(axis);
}

void CInputCaptureProtocol::frame() {
    if (active)
        active->frame();
}

void CInputCaptureResource::updateKeymap() {
    Debug::log(LOG, "[input-capture] Got new keymap, reseting keyboard");
    eis->resetKeyboard();
}
