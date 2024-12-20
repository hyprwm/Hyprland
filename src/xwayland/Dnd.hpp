#pragma once

#include "../protocols/types/DataDevice.hpp"
#include <wayland-server-protocol.h>

#define XDND_VERSION 5

class CXWaylandSurface;

class CX11DataOffer : public IDataOffer {
  public:
    CX11DataOffer()  = default;
    ~CX11DataOffer() = default;

    virtual eDataSourceType          type();
    virtual SP<CWLDataOfferResource> getWayland();
    virtual SP<CX11DataOffer>        getX11();
    virtual SP<IDataSource>          getSource();
    virtual void                     markDead();

    WP<IDataSource>                  source;
    WP<CX11DataOffer>                self;
    WP<CXWaylandSurface>             xwaylandSurface;

    bool                             dead     = false;
    bool                             accepted = false;
    bool                             recvd    = false;

    uint32_t                         actions = 0;
};

class CX11DataSource : public IDataSource {
  public:
    CX11DataSource()  = default;
    ~CX11DataSource() = default;

    virtual std::vector<std::string> mimes();
    virtual void                     send(const std::string& mime, uint32_t fd);
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

    bool                             used       = false;
    bool                             dnd        = true;
    bool                             dndSuccess = false;
    bool                             dropped    = false;

    WP<CX11DataSource>               self;

    std::vector<std::string>         mimeTypes;
    uint32_t                         supportedActions = 0;
};

class CX11DataDevice : public IDataDevice {
  public:
    CX11DataDevice() = default;

    virtual SP<CWLDataDeviceResource> getWayland();
    virtual SP<CX11DataDevice>        getX11();
    virtual void                      sendDataOffer(SP<IDataOffer> offer);
    virtual void                      sendEnter(uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& local, SP<IDataOffer> offer);
    virtual void                      sendLeave();
    virtual void                      sendMotion(uint32_t timeMs, const Vector2D& local);
    virtual void                      sendDrop();
    virtual void                      sendSelection(SP<IDataOffer> offer);
    virtual eDataSourceType           type();

    WP<CX11DataDevice>                self;

  private:
    WP<CXWaylandSurface> lastSurface;
    WP<IDataOffer>       lastOffer;
    Vector2D             lastSurfaceCoords;
    uint32_t             lastTime = 0;
};
