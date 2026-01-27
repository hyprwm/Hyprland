#pragma once

#include "../protocols/types/DataDevice.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/input/InputManager.hpp"
#include <wayland-server-protocol.h>
#include <hyprutils/os/FileDescriptor.hpp>
#ifndef NO_XWAYLAND
#include <xcb/xcb.h>
#endif

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

    WP<IDataSource>                  m_source;
    WP<CX11DataOffer>                m_self;
    WP<CXWaylandSurface>             m_xwaylandSurface;
};

class CX11DataSource : public IDataSource {
  public:
    CX11DataSource()  = default;
    ~CX11DataSource() = default;

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

    bool                             m_dnd        = true;
    bool                             m_dndSuccess = false;
    bool                             m_dropped    = false;

    std::vector<std::string>         m_mimeTypes;
    uint32_t                         m_supportedActions = 0;
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
    void                              forceCleanupDnd();

    WP<CX11DataDevice>                m_self;

  private:
    void cleanupState();
#ifndef NO_XWAYLAND
    xcb_window_t getProxyWindow(xcb_window_t window);
    void         sendDndEvent(xcb_window_t targetWindow, xcb_atom_t type, xcb_client_message_data_t& data);
#endif
    WP<CXWaylandSurface> m_lastSurface;
    WP<IDataOffer>       m_lastOffer;
    Vector2D             m_lastSurfaceCoords;
    uint32_t             m_lastTime = 0;
};
