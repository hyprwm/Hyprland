#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "wlr-data-control-unstable-v1.hpp"
#include "types/DataDevice.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CWLRDataControlManagerResource;
class CWLRDataSource;
class CWLRDataDevice;
class CWLRDataOffer;

class CWLRDataOffer {
  public:
    CWLRDataOffer(SP<CZwlrDataControlOfferV1> resource_, SP<IDataSource> source);

    bool            good();
    void            sendData();

    bool            m_dead    = false;
    bool            m_primary = false;

    WP<IDataSource> m_source;

  private:
    SP<CZwlrDataControlOfferV1> m_resource;

    friend class CWLRDataDevice;
};

class CWLRDataSource : public IDataSource {
  public:
    CWLRDataSource(SP<CZwlrDataControlSourceV1> resource_, SP<CWLRDataDevice> device_);
    ~CWLRDataSource();
    static SP<CWLRDataSource>        fromResource(wl_resource*);

    bool                             good();

    virtual std::vector<std::string> mimes();
    virtual void                     send(const std::string& mime, Hyprutils::OS::CFileDescriptor fd);
    virtual void                     accepted(const std::string& mime);
    virtual void                     cancelled();
    virtual void                     error(uint32_t code, const std::string& msg);

    std::vector<std::string>         m_mimeTypes;
    WP<CWLRDataSource>               m_self;
    WP<CWLRDataDevice>               m_device;

  private:
    SP<CZwlrDataControlSourceV1> m_resource;
};

class CWLRDataDevice {
  public:
    CWLRDataDevice(SP<CZwlrDataControlDeviceV1> resource_);

    bool               good();
    wl_client*         client();
    void               sendInitialSelections();

    void               sendDataOffer(SP<CWLRDataOffer> offer);
    void               sendSelection(SP<CWLRDataOffer> selection);
    void               sendPrimarySelection(SP<CWLRDataOffer> selection);

    WP<CWLRDataDevice> self;

  private:
    SP<CZwlrDataControlDeviceV1> m_resource;
    wl_client*                   m_client = nullptr;

    friend class CDataDeviceWLRProtocol;
};

class CWLRDataControlManagerResource {
  public:
    CWLRDataControlManagerResource(SP<CZwlrDataControlManagerV1> resource_);

    bool                            good();

    WP<CWLRDataDevice>              m_device;
    std::vector<WP<CWLRDataSource>> m_sources;

  private:
    SP<CZwlrDataControlManagerV1> m_resource;
};

class CDataDeviceWLRProtocol : public IWaylandProtocol {
  public:
    CDataDeviceWLRProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CWLRDataControlManagerResource* resource);
    void destroyResource(CWLRDataSource* resource);
    void destroyResource(CWLRDataDevice* resource);
    void destroyResource(CWLRDataOffer* resource);

    //
    std::vector<SP<CWLRDataControlManagerResource>> m_managers;
    std::vector<SP<CWLRDataSource>>                 m_sources;
    std::vector<SP<CWLRDataDevice>>                 m_devices;
    std::vector<SP<CWLRDataOffer>>                  m_offers;

    //
    void setSelection(SP<IDataSource> source, bool primary);
    void sendSelectionToDevice(SP<CWLRDataDevice> dev, SP<IDataSource> sel, bool primary);

    //
    SP<CWLRDataDevice> dataDeviceForClient(wl_client*);

    friend class CSeatManager;
    friend class CWLRDataControlManagerResource;
    friend class CWLRDataSource;
    friend class CWLRDataDevice;
    friend class CWLRDataOffer;
};

namespace PROTO {
    inline UP<CDataDeviceWLRProtocol> dataWlr;
};
