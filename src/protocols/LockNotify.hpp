#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "hyprland-lock-notify-v1.hpp"

class CEventLoopTimer;

class CHyprlandLockNotification {
  public:
    CHyprlandLockNotification(SP<CHyprlandLockNotificationV1> resource_);
    ~CHyprlandLockNotification() = default;

    bool good();
    void onLocked();
    void onUnlocked();

  private:
    SP<CHyprlandLockNotificationV1> m_resource;
    bool                            m_locked = false;
};

class CLockNotifyProtocol : public IWaylandProtocol {
  public:
    CLockNotifyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onLocked();
    void         onUnlocked();

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyNotification(CHyprlandLockNotification* notif);
    void onGetNotification(CHyprlandLockNotifierV1* pMgr, uint32_t id);

    bool m_isLocked = false;

    //
    std::vector<UP<CHyprlandLockNotifierV1>>   m_managers;
    std::vector<SP<CHyprlandLockNotification>> m_notifications;

    friend class CHyprlandLockNotification;
};

namespace PROTO {
    inline UP<CLockNotifyProtocol> lockNotify;
};
