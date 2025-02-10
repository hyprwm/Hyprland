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
    void onActivity();

    bool inhibitorsAreObeyed() const;

  private:
    SP<CExtIdleNotificationV1> resource;
    uint32_t                   timeoutMs = 0;
    SP<CEventLoopTimer>        timer;

    bool                       idled          = false;
    bool                       obeyInhibitors = false;

    void                       updateTimer();
};

class CIdleNotifyProtocol : public IWaylandProtocol {
  public:
    CIdleNotifyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onActivity();
    void         setInhibit(bool inhibited);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyNotification(CExtIdleNotification* notif);
    void onGetNotification(CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat, bool obeyInhibitors);

    bool isInhibited = false;

    //
    std::vector<UP<CExtIdleNotifierV1>>   m_vManagers;
    std::vector<SP<CExtIdleNotification>> m_vNotifications;

    friend class CExtIdleNotification;
};

namespace PROTO {
    inline UP<CIdleNotifyProtocol> idle;
};
