#pragma once

#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "ext-idle-notify-v1.hpp"

class CEventLoopTimer;

class CExtIdleNotification {
  public:
    CExtIdleNotification(SP<CExtIdleNotificationV1> resource_, uint32_t timeoutMs, bool obeyInhibitors);
    ~CExtIdleNotification();

    bool good();
    void onTimerFired();

    bool inhibitorsAreObeyed() const;

  private:
    SP<CExtIdleNotificationV1> m_resource;
    uint32_t                   m_timeoutMs = 0;
    SP<CEventLoopTimer>        m_timer;

    bool                       m_idled          = false;
    bool                       m_obeyInhibitors = false;

    void                       reset();
    void                       update();
    void                       update(uint32_t elapsedMs);

    friend class CIdleNotifyProtocol;
};

class CIdleNotifyProtocol : public IWaylandProtocol {
  public:
    CIdleNotifyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onActivity();
    void         setInhibit(bool inhibited);
    void         setTimers(uint32_t elapsedMs);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyNotification(CExtIdleNotification* notif);
    void onGetNotification(CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat, bool obeyInhibitors);

    bool isInhibited = false;

    //
    std::vector<UP<CExtIdleNotifierV1>>   m_managers;
    std::vector<SP<CExtIdleNotification>> m_notifications;

    friend class CExtIdleNotification;
};

namespace PROTO {
    inline UP<CIdleNotifyProtocol> idle;
};
