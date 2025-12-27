#include "InputCapture.hpp"

#include "Compositor.hpp"
#include "config/ConfigValue.hpp"
#include "debug/log/Logger.hpp"
#include "hyprland-input-capture-v1.hpp"
#include "managers/HookSystemManager.hpp"
#include "managers/permissions/DynamicPermissionManager.hpp"
#include "protocols/WaylandProtocol.hpp"
#include "render/Renderer.hpp"
#include <algorithm>
#include <cairo.h>
#include <cstdint>
#include <fcntl.h>
#include <glaze/core/context.hpp>
#include <glaze/util/parse.hpp>
#include <hyprlang.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <libeis.h>
#include <optional>
#include <string>

static int eisCounter = 0;

CInputCaptureResource::CInputCaptureResource(SP<CHyprlandInputCaptureV1> resource_, std::string handle) : m_sessionId(handle), m_resource(resource_) {
    if UNLIKELY (!good())
        return;
    Log::logger->log(Log::INFO, "[input-capture]({}) new session", m_sessionId.c_str());

    m_resource->setOnDestroy([this](CHyprlandInputCaptureV1* r) { PROTO::inputCapture->destroyResource(this); }); //Remove & free this session

    m_resource->setEnable([this](CHyprlandInputCaptureV1* r) { onEnable(); });
    m_resource->setAddBarrier(
        [this](CHyprlandInputCaptureV1* r, uint32_t zoneSet, uint32_t id, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) { onAddBarrier(zoneSet, id, x1, y1, x2, y2); });
    m_resource->setDisable([this](CHyprlandInputCaptureV1* r) { disable(); });
    m_resource->setRelease([this](CHyprlandInputCaptureV1* r, uint32_t activationId, double x, double y) { onRelease(activationId, x, y); });
    m_resource->setClearBarriers([this](CHyprlandInputCaptureV1* r) { onClearBarriers(); });

    m_eis = makeUnique<CEis>("eis-" + std::to_string(eisCounter++));

    m_resource->sendEisFd(m_eis->getFileDescriptor());

    m_monitorCallback = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) {
        onClearBarriers();
        disable();
        m_eis->resetPointer();
    });
}

CInputCaptureResource::~CInputCaptureResource() {
    if (m_status == CLIENT_STATUS_ACTIVATED)
        PROTO::inputCapture->forceRelease();

    Log::logger->log(Log::INFO, "[input-capture]({}) session destroyed", m_sessionId.c_str());
    g_pHookSystem->unhook(m_monitorCallback);
    PROTO::inputCapture->clearBarriers(m_sessionId);
};

bool CInputCaptureResource::enabled() {
    return m_status == CLIENT_STATUS_ENABLED;
}

bool CInputCaptureResource::good() {
    return m_resource->resource();
}

void CInputCaptureResource::onEnable() {
    Log::logger->log(Log::INFO, "[input-capture]({}) session enabled", m_sessionId.c_str());
    m_status = CLIENT_STATUS_ENABLED;
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
	static auto PENFORCEBARRIERS = CConfigValue<Hyprlang::INT>("inputcapture:enforce_barriers");
    bool valid = isBarrierValid(x1, y1, x2, y2);

    if (!valid) {
        Log::logger->log(Log::INFO, "[input-capture]({}) Barrier {} is invalid [{}, {}], [{}, {}]", m_sessionId.c_str(), id, x1, y1, x2, y2);

		if (*PENFORCEBARRIERS) {
        	m_resource->error(HYPRLAND_INPUT_CAPTURE_V1_ERROR_INVALID_BARRIER, "The barrier id " + std::to_string(id) + " is invalid");
			return;
		}
    }

    Log::logger->log(Log::INFO, "[input-capture]({}) Barrier {} [{}, {}], [{}, {}] added", m_sessionId.c_str(), id, x1, y1, x2, y2);

    PROTO::inputCapture->addBarrier({.sessionId = m_sessionId, .id = id, .x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2});
}

void CInputCaptureResource::onDisable() {
    disable();
}

void CInputCaptureResource::onRelease(uint32_t activationId_, double x, double y) {
    if (activationId_ != m_activationId) { // If id is invalid we still want to release the mouse to avoid any issue
        Log::logger->log(Log::WARN, "[input-capture]({}) Invalid activation id {} expected {}", m_sessionId.c_str(), activationId_, m_activationId);
    }

    deactivate();

    if (x != -1 && y != -1)
        g_pCompositor->warpCursorTo({x, y}, true);
    g_pHyprRenderer->ensureCursorRenderingMode();
}

void CInputCaptureResource::onClearBarriers() {
    PROTO::inputCapture->clearBarriers(m_sessionId);
}

bool CInputCaptureResource::activate(double x, double y, uint32_t borderId) {
    if (m_status != CLIENT_STATUS_ENABLED)
        return false;

    const auto PERM = g_pDynamicPermissionManager->clientPermissionMode(m_resource->client(), PERMISSION_TYPE_INPUT_CAPTURE);
    if (PERM != PERMISSION_RULE_ALLOW_MODE_ALLOW)
        return false;

    m_activationId += 5;
    m_status = CLIENT_STATUS_ACTIVATED;
    Log::logger->log(Log::INFO, "[input-capture]({}) Input captured, activationId: {}, borderId: {}, x: {}, y: {}", m_sessionId.c_str(), m_activationId, borderId, x, y);
    m_eis->startEmulating(m_activationId);
    g_pHyprRenderer->ensureCursorRenderingMode();
    m_resource->sendActivated(m_activationId, x, y, borderId);

    return true;
}

void CInputCaptureResource::deactivate() {
    if (m_status != CLIENT_STATUS_ACTIVATED)
        return;

    Log::logger->log(Log::INFO, "[input-capture]({}) Input released", m_sessionId.c_str());
    m_status = CLIENT_STATUS_ENABLED;
    m_eis->stopEmulating();
    PROTO::inputCapture->release();

    m_resource->sendDeactivated(m_activationId);
}

void CInputCaptureResource::disable() {
    if (m_status == CLIENT_STATUS_ACTIVATED)
        deactivate();
    if (m_status != CLIENT_STATUS_ENABLED)
        return;
    m_status = CLIENT_STATUS_CREATED;

    m_resource->sendDisabled();
}

void CInputCaptureResource::motion(double dx, double dy) {
    m_eis->sendMotion(dx, dy);
}

void CInputCaptureResource::key(uint32_t key, bool pressed) {
    m_eis->sendKey(key, pressed);
}

void CInputCaptureResource::modifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group) {
    m_eis->sendModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void CInputCaptureResource::button(uint32_t button, bool pressed) {
    m_eis->sendButton(button, pressed);
}

void CInputCaptureResource::axis(bool axis, double value) {
    int32_t x = 0;
    int32_t y = 0;

    if (axis)
        x = value;
    else
        y = value;

    m_eis->sendScrollDelta(x, y);
}

void CInputCaptureResource::axisValue120(bool axis, int32_t value120) {
    int32_t x = 0;
    int32_t y = 0;

    if (axis)
        x = value120;
    else
        y = value120;

    m_eis->sendScrollDiscrete(x, y);
}

void CInputCaptureResource::axisStop(bool axis) {
    m_eis->sendScrollStop(axis, !axis);
}

void CInputCaptureResource::frame() {
    m_eis->sendPointerFrame();
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
    for (const auto& barrier : barriers) {
        auto session = getSession(barrier.sessionId);
        if (!session.has_value())
            continue;
        if (!session.value()->enabled())
            continue;

        if (testCollision(barrier, px, py, nx, ny))
            return std::optional(barrier);
    }

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

	Log::logger->log(Log::INFO, "New InputCapture at id {}", id);
}

void CInputCaptureProtocol::destroyResource(CInputCaptureResource* resource) {
    std::erase_if(m_Sessions, [resource](const auto& other) { return other->m_sessionId == resource->m_sessionId; });
}

bool CInputCaptureProtocol::isCaptured() {
    return active != nullptr;
}

void CInputCaptureProtocol::updateKeymap() {
    for (auto& session : m_Sessions) {
        session->updateKeymap();
    }
}

void CInputCaptureProtocol::motion(const Vector2D& absolutePosition, const Vector2D& delta) {
    auto [x, y]   = absolutePosition;
    auto [dx, dy] = delta;

    auto matched = isColliding(x, y, x - dx, y - dy);
    if (matched.has_value()) {
        auto session = getSession(matched->sessionId);
        if (!session.has_value()) {
            Log::logger->log(Log::ERR, "Cannot find session {} for triggering barrier {}", matched.value().id, matched.value().sessionId);
            return;
        }
        if (session.value()->activate(x, y, matched.value().id))
            active = session.value();
    }

    if (active)
        active->motion(dx, dy);
}

std::optional<SP<CInputCaptureResource>> CInputCaptureProtocol::getSession(std::string sessionId) {
    auto result = std::ranges::find_if(m_Sessions, [sessionId](SP<CInputCaptureResource> elem) { return sessionId == elem->m_sessionId; });
    if (result == m_Sessions.end())
        return std::nullopt;

    return std::optional(*result);
}

void CInputCaptureProtocol::addBarrier(SBarrier barrier) {
    barriers.emplace_back(barrier);
}

void CInputCaptureProtocol::clearBarriers(std::string sessionId) {
    std::erase_if(barriers, [sessionId](SBarrier b) { return b.sessionId == sessionId; });
    Log::logger->log(Log::INFO, "[input-capture]({}) Barriers cleared", sessionId.c_str());
}

void CInputCaptureProtocol::release() {
    active = nullptr;
}

void CInputCaptureProtocol::forceRelease() {
    Log::logger->log(Log::INFO, "[input-capture] Force release input capture");
    if (active) {
        auto cpy = active; //Because deactivate will put active to nullptr
        cpy->deactivate();
        cpy->disable();
    }
    release();
}

void CInputCaptureProtocol::key(uint32_t keyCode, wl_keyboard_key_state state) {
    if (active)
        active->key(keyCode, state);
}

void CInputCaptureProtocol::modifiers(uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    if (active)
        active->modifiers(mods_depressed, mods_latched, mods_locked, group);
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
    Log::logger->log(Log::INFO, "[input-capture] Got new keymap, reseting keyboard");
    m_eis->resetKeyboard();
}
