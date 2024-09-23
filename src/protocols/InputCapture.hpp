#pragma once

#include "managers/input/Eis.hpp"
#include "helpers/memory/Memory.hpp"
#include "hyprland-input-capture-v1.hpp"
#include "../protocols/WaylandProtocol.hpp"
#include <cstdint>
#include <hyprutils/math/Vector2D.hpp>
#include <optional>
#include <wayland-server.h>

enum eClientStatus : uint8_t {
    CLIENT_STATUS_CREATED,   //Is ready to be activated
    CLIENT_STATUS_ENABLED,   //Is ready for receiving inputs
    CLIENT_STATUS_ACTIVATED, //Currently receiving inputs
    CLIENT_STATUS_STOPPED    //Can no longer be activated
};

struct SBarrier {
    std::string sessionId = "";
    uint        id        = 0;
    int         x1        = 0;
    int         y1        = 0;
    int         x2        = 0;
    int         y2        = 0;
};

class CInputCaptureResource {
  public:
    CInputCaptureResource(SP<CHyprlandInputCaptureV1> resource_, std::string handle);
    ~CInputCaptureResource();

    void motion(double dx, double dy);
    void key(uint32_t key, bool pressed);
    void modifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    void button(uint32_t button, bool pressed);
    void axis(bool axis, double value);
    void axisValue120(bool axis, int32_t value120);
    void axisStop(bool axis);
    void frame();
    void updateKeymap();

    bool activate(double x, double y, uint32_t borderId);
    void disable();

    bool enabled();
    bool good();

    //
    std::string m_sessionId;

  private:
    void deactivate();

    void onEnable();
    void onAddBarrier(uint32_t zoneSet, uint32_t id, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);
    void onDisable();
    void onRelease(uint32_t activationId, double x, double y);
    void onClearBarriers();

    //
    SP<CHyprlandInputCaptureV1> m_resource;
    UP<CEis>                    m_eis;

    uint32_t                    m_activationId = 0;
    eClientStatus               m_status       = eClientStatus::CLIENT_STATUS_CREATED;
    SP<HOOK_CALLBACK_FN>        m_monitorCallback;
};

class CInputCaptureProtocol : public IWaylandProtocol {
  public:
    CInputCaptureProtocol(const wl_interface* iface, const int& ver, const std::string& name);
    void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
    void destroyResource(CInputCaptureResource* pointer);

    void addBarrier(SBarrier barrier);
    void clearBarriers(std::string sessionId);

    void updateKeymap();

    bool isCaptured();

    void motion(const Vector2D& absolutePosition, const Vector2D& delta);
    void key(uint32_t keyCode, wl_keyboard_key_state state);
    void modifiers(uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
    void button(uint32_t button, wl_pointer_button_state state);
    void axis(wl_pointer_axis axis, double value);
    void axisValue120(wl_pointer_axis axis, int32_t value120);
    void axisStop(wl_pointer_axis axis);
    void frame();
    void forceRelease();

    void release();

  private:
    std::vector<UP<CHyprlandInputCaptureManagerV1>> m_vManagers;
    std::vector<SP<CInputCaptureResource>>          m_Sessions;
    SP<CInputCaptureResource>                       active = nullptr;
    std::vector<SBarrier>                           barriers;

    //
    void                                     onCreateSession(CHyprlandInputCaptureManagerV1* pMgr, uint32_t id, std::string handle);
    std::optional<SBarrier>                  isColliding(double px, double py, double nx, double ny);
    std::optional<SP<CInputCaptureResource>> getSession(std::string sessionId);
};

namespace PROTO {
    inline UP<CInputCaptureProtocol> inputCapture;
}
