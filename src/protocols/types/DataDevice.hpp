#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../../helpers/signal/Signal.hpp"
#include <wayland-server-protocol.h>
#include "../../helpers/memory/Memory.hpp"

class CWLDataOfferResource;
class CX11DataOffer;
class CX11DataDevice;
class CWLDataDeviceResource;
class CWLSurfaceResource;

enum eDataSourceType : uint8_t {
    DATA_SOURCE_TYPE_WAYLAND = 0,
    DATA_SOURCE_TYPE_X11,
};

class IDataSource {
  public:
    IDataSource()          = default;
    virtual ~IDataSource() = default;

    virtual std::vector<std::string> mimes()                                    = 0;
    virtual void                     send(const std::string& mime, uint32_t fd) = 0;
    virtual void                     accepted(const std::string& mime)          = 0;
    virtual void                     cancelled()                                = 0;
    virtual bool                     hasDnd();
    virtual bool                     dndDone();
    virtual void                     sendDndFinished();
    virtual bool                     used();
    virtual void                     markUsed();
    virtual void                     error(uint32_t code, const std::string& msg) = 0;
    virtual eDataSourceType          type();
    virtual uint32_t                 actions(); // wl_data_device_manager.dnd_action
    virtual void                     sendDndDropPerformed();
    virtual void                     sendDndAction(wl_data_device_manager_dnd_action a);

    struct {
        CSignal destroy;
    } events;

  private:
    bool wasUsed = false;
};

class IDataOffer {
  public:
    IDataOffer()          = default;
    virtual ~IDataOffer() = default;

    virtual eDataSourceType          type()       = 0;
    virtual SP<CWLDataOfferResource> getWayland() = 0;
    virtual SP<CX11DataOffer>        getX11()     = 0;
    virtual SP<IDataSource>          getSource()  = 0;
    virtual void                     markDead();
};

class IDataDevice {
  public:
    IDataDevice()          = default;
    virtual ~IDataDevice() = default;

    virtual SP<CWLDataDeviceResource> getWayland()                                                                                         = 0;
    virtual SP<CX11DataDevice>        getX11()                                                                                             = 0;
    virtual void                      sendDataOffer(SP<IDataOffer> offer)                                                                  = 0;
    virtual void                      sendEnter(uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& local, SP<IDataOffer> offer) = 0;
    virtual void                      sendLeave()                                                                                          = 0;
    virtual void                      sendMotion(uint32_t timeMs, const Vector2D& local)                                                   = 0;
    virtual void                      sendDrop()                                                                                           = 0;
    virtual void                      sendSelection(SP<IDataOffer> offer)                                                                  = 0;
    virtual eDataSourceType           type()                                                                                               = 0;
};
