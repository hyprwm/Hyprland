#pragma once

/*
    Implementations for:
     - wl_data_offer
     - wl_data_source
     - wl_data_device
     - wl_data_device_manager
*/

#include <memory>
#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include <wayland-server-protocol.h>
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/Vector2D.hpp"
#include "../types/DataDevice.hpp"

class CWLDataDeviceResource;
class CWLDataDeviceManagerResource;
class CWLDataSourceResource;
class CWLDataOfferResource;

class CMonitor;

class CWLDataOfferResource {
  public:
    CWLDataOfferResource(SP<CWlDataOffer> resource_, SP<IDataSource> source_);

    bool            good();
    void            sendData();

    WP<IDataSource> source;

    bool            dead     = false;
    bool            accepted = false;
    bool            recvd    = false;

    uint32_t        actions = 0;

  private:
    SP<CWlDataOffer> resource;
    wl_client*       pClient = nullptr;

    friend class CWLDataDeviceResource;
};

class CWLDataSourceResource : public IDataSource {
  public:
    CWLDataSourceResource(SP<CWlDataSource> resource_, SP<CWLDataDeviceResource> device_);
    ~CWLDataSourceResource();
    static SP<CWLDataSourceResource> fromResource(wl_resource*);

    bool                             good();

    virtual std::vector<std::string> mimes();
    virtual void                     send(const std::string& mime, uint32_t fd);
    virtual void                     accepted(const std::string& mime);
    virtual void                     cancelled();
    virtual bool                     hasDnd();
    virtual bool                     dndDone();
    virtual void                     error(uint32_t code, const std::string& msg);

    void                             sendDndDropPerformed();
    void                             sendDndFinished();
    void                             sendDndAction(wl_data_device_manager_dnd_action a);

    bool                             used       = false;
    bool                             dnd        = false;
    bool                             dndSuccess = false;

    WP<CWLDataDeviceResource>        device;
    WP<CWLDataSourceResource>        self;

    std::vector<std::string>         mimeTypes;
    uint32_t                         actions = 0;

  private:
    SP<CWlDataSource> resource;
    wl_client*        pClient = nullptr;

    friend class CWLDataDeviceProtocol;
};

class CWLDataDeviceResource {
  public:
    CWLDataDeviceResource(SP<CWlDataDevice> resource_);

    bool                      good();
    wl_client*                client();

    void                      sendDataOffer(SP<CWLDataOfferResource> offer);
    void                      sendEnter(uint32_t serial, wlr_surface* surf, const Vector2D& local, SP<CWLDataOfferResource> offer);
    void                      sendLeave();
    void                      sendMotion(uint32_t timeMs, const Vector2D& local);
    void                      sendDrop();
    void                      sendSelection(SP<CWLDataOfferResource> offer);

    WP<CWLDataDeviceResource> self;

  private:
    SP<CWlDataDevice> resource;
    wl_client*        pClient = nullptr;

    friend class CWLDataDeviceProtocol;
};

class CWLDataDeviceManagerResource {
  public:
    CWLDataDeviceManagerResource(SP<CWlDataDeviceManager> resource_);

    bool                                   good();

    WP<CWLDataDeviceResource>              device;
    std::vector<WP<CWLDataSourceResource>> sources;

  private:
    SP<CWlDataDeviceManager> resource;
};

class CWLDataDeviceProtocol : public IWaylandProtocol {
  public:
    CWLDataDeviceProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    // renders and damages the dnd icon, if present
    void renderDND(CMonitor* pMonitor, timespec* when);
    // for inputmgr to force refocus
    // TODO: move handling to seatmgr
    bool dndActive();

  private:
    void destroyResource(CWLDataDeviceManagerResource* resource);
    void destroyResource(CWLDataDeviceResource* resource);
    void destroyResource(CWLDataSourceResource* resource);
    void destroyResource(CWLDataOfferResource* resource);

    //
    std::vector<SP<CWLDataDeviceManagerResource>> m_vManagers;
    std::vector<SP<CWLDataDeviceResource>>        m_vDevices;
    std::vector<SP<CWLDataSourceResource>>        m_vSources;
    std::vector<SP<CWLDataOfferResource>>         m_vOffers;

    //

    void onDestroyDataSource(WP<CWLDataSourceResource> source);
    void setSelection(SP<IDataSource> source);
    void sendSelectionToDevice(SP<CWLDataDeviceResource> dev, SP<IDataSource> sel);
    void updateSelection();
    void onKeyboardFocus();

    struct {
        WP<CWLDataDeviceResource> focusedDevice;
        WP<CWLDataSourceResource> currentSource;
        wlr_surface*              dndSurface       = nullptr;
        wlr_surface*              originSurface    = nullptr; // READ-ONLY
        bool                      overriddenCursor = false;
        DYNLISTENER(dndSurfaceDestroy);
        DYNLISTENER(dndSurfaceCommit);

        // for ending a dnd
        SP<HOOK_CALLBACK_FN> mouseMove;
        SP<HOOK_CALLBACK_FN> mouseButton;
        SP<HOOK_CALLBACK_FN> touchUp;
        SP<HOOK_CALLBACK_FN> touchMove;
    } dnd;

    void abortDrag();
    void initiateDrag(WP<CWLDataSourceResource> currentSource, wlr_surface* dragSurface, wlr_surface* origin);
    void updateDrag();
    void dropDrag();
    void completeDrag();
    void resetDndState();

    //
    SP<CWLDataDeviceResource> dataDeviceForClient(wl_client*);

    friend class CSeatManager;
    friend class CWLDataDeviceManagerResource;
    friend class CWLDataDeviceResource;
    friend class CWLDataSourceResource;
    friend class CWLDataOfferResource;

    struct {
        CHyprSignalListener onKeyboardFocusChange;
    } listeners;
};

namespace PROTO {
    inline UP<CWLDataDeviceProtocol> data;
};
