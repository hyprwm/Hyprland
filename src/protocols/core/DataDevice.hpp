#pragma once

/*
    Implementations for:
     - wl_data_offer
     - wl_data_source
     - wl_data_device
     - wl_data_device_manager
*/

#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include <wayland-server-protocol.h>
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/math/Math.hpp"
#include "../../helpers/time/Time.hpp"
#include "../types/DataDevice.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CWLDataDeviceResource;
class CWLDataDeviceManagerResource;
class CWLDataSourceResource;
class CWLDataOfferResource;

class CWLSurfaceResource;
class CMonitor;

class CWLDataOfferResource : public IDataOffer {
  public:
    CWLDataOfferResource(SP<CWlDataOffer> resource_, SP<IDataSource> source_);
    ~CWLDataOfferResource();

    bool                             good();
    void                             sendData();

    virtual eDataSourceType          type();
    virtual SP<CWLDataOfferResource> getWayland();
    virtual SP<CX11DataOffer>        getX11();
    virtual SP<IDataSource>          getSource();

    WP<IDataSource>                  m_source;
    WP<CWLDataOfferResource>         m_self;

    bool                             m_dead     = false;
    bool                             m_accepted = false;
    bool                             m_recvd    = false;

  private:
    SP<CWlDataOffer> m_resource;

    friend class CWLDataDeviceResource;
};

class CWLDataSourceResource : public IDataSource {
  public:
    CWLDataSourceResource(SP<CWlDataSource> resource_, SP<CWLDataDeviceResource> device_);
    ~CWLDataSourceResource();
    static SP<CWLDataSourceResource> fromResource(wl_resource*);

    bool                             good();

    virtual std::vector<std::string> mimes();
    virtual void                     send(const std::string& mime, Hyprutils::OS::CFileDescriptor fd);
    virtual void                     accepted(const std::string& mime);
    virtual void                     cancelled();
    virtual bool                     hasDnd();
    virtual bool                     dndDone();
    virtual void                     error(uint32_t code, const std::string& msg);
    virtual void                     sendDndFinished();
    virtual uint32_t                 actions(); // wl_data_device_manager.dnd_action
    virtual eDataSourceType          type();
    virtual void                     sendDndDropPerformed();
    virtual void                     sendDndAction(wl_data_device_manager_dnd_action a);

    bool                             m_used       = false;
    bool                             m_dnd        = false;
    bool                             m_dndSuccess = false;
    bool                             m_dropped    = false;

    WP<CWLDataDeviceResource>        m_device;
    WP<CWLDataSourceResource>        m_self;

    std::vector<std::string>         m_mimeTypes;
    uint32_t                         m_supportedActions = 0;

  private:
    SP<CWlDataSource> m_resource;

    friend class CWLDataDeviceProtocol;
};

class CWLDataDeviceResource : public IDataDevice {
  public:
    CWLDataDeviceResource(SP<CWlDataDevice> resource_);

    bool                              good();
    wl_client*                        client();

    virtual SP<CWLDataDeviceResource> getWayland();
    virtual SP<CX11DataDevice>        getX11();
    virtual void                      sendDataOffer(SP<IDataOffer> offer);
    virtual void                      sendEnter(uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& local, SP<IDataOffer> offer);
    virtual void                      sendLeave();
    virtual void                      sendMotion(uint32_t timeMs, const Vector2D& local);
    virtual void                      sendDrop();
    virtual void                      sendSelection(SP<IDataOffer> offer);
    virtual eDataSourceType           type();

    WP<CWLDataDeviceResource>         m_self;

  private:
    SP<CWlDataDevice> m_resource;
    wl_client*        m_client = nullptr;

    friend class CWLDataDeviceProtocol;
};

class CWLDataDeviceManagerResource {
  public:
    CWLDataDeviceManagerResource(SP<CWlDataDeviceManager> resource_);

    bool                                   good();

    WP<CWLDataDeviceResource>              m_device;
    std::vector<WP<CWLDataSourceResource>> m_sources;

  private:
    SP<CWlDataDeviceManager> m_resource;
};

class CWLDataDeviceProtocol : public IWaylandProtocol {
  public:
    CWLDataDeviceProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    // renders and damages the dnd icon, if present
    void renderDND(PHLMONITOR pMonitor, const Time::steady_tp& when);
    // for inputmgr to force refocus
    // TODO: move handling to seatmgr
    bool dndActive();

    // called on an escape key pressed, for moments where it gets stuck
    void abortDndIfPresent();

  private:
    void destroyResource(CWLDataDeviceManagerResource* resource);
    void destroyResource(CWLDataDeviceResource* resource);
    void destroyResource(CWLDataSourceResource* resource);
    void destroyResource(CWLDataOfferResource* resource);

    //
    std::vector<SP<CWLDataDeviceManagerResource>> m_managers;
    std::vector<SP<CWLDataDeviceResource>>        m_devices;
    std::vector<SP<CWLDataSourceResource>>        m_sources;
    std::vector<SP<CWLDataOfferResource>>         m_offers;

    //

    void onDestroyDataSource(WP<CWLDataSourceResource> source);
    void setSelection(SP<IDataSource> source);
    void sendSelectionToDevice(SP<IDataDevice> dev, SP<IDataSource> sel);
    void updateSelection();
    void onKeyboardFocus();
    void onDndPointerFocus();

    struct {
        WP<IDataDevice>        focusedDevice;
        WP<IDataSource>        currentSource;
        WP<CWLSurfaceResource> dndSurface;
        WP<CWLSurfaceResource> originSurface;
        bool                   overriddenCursor = false;
        CHyprSignalListener    dndSurfaceDestroy;
        CHyprSignalListener    dndSurfaceCommit;

        // for ending a dnd
        SP<HOOK_CALLBACK_FN> mouseMove;
        SP<HOOK_CALLBACK_FN> mouseButton;
        SP<HOOK_CALLBACK_FN> touchUp;
        SP<HOOK_CALLBACK_FN> touchMove;
    } m_dnd;

    void abortDrag();
    void initiateDrag(WP<CWLDataSourceResource> currentSource, SP<CWLSurfaceResource> dragSurface, SP<CWLSurfaceResource> origin);
    void updateDrag();
    void dropDrag();
    void completeDrag();
    void cleanupDndState(bool resetDevice, bool resetSource, bool simulateInput);
    bool wasDragSuccessful();

    //
    SP<IDataDevice> dataDeviceForClient(wl_client*);

    friend class CSeatManager;
    friend class CWLDataDeviceManagerResource;
    friend class CWLDataDeviceResource;
    friend class CWLDataSourceResource;
    friend class CWLDataOfferResource;

    struct {
        CHyprSignalListener onKeyboardFocusChange;
        CHyprSignalListener onDndPointerFocusChange;
    } m_listeners;
};

namespace PROTO {
    inline UP<CWLDataDeviceProtocol> data;
};
