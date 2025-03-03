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
    CWLRDataOffer(SP<CZwlrDataControlOfferV1> resource_, SP<CIDataSource> source);

    bool             good();
    void             sendData();

    bool             dead    = false;
    bool             primary = false;

    WP<CIDataSource> source;

  private:
    SP<CZwlrDataControlOfferV1> resource;

    friend class CWLRDataDevice;
};

class CWLRDataSource : publicIataSource {
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

    std::vector<std::string>         mimeTypes;
    WP<CWLRDataSource>               self;
    WP<CWLRDataDevice>               device;

  private:
    SP<CZwlrDataControlSourceV1> resource;
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
    SP<CZwlrDataControlDeviceV1> resource;
    wl_client*                   pClient = nullptr;

    friend class CDataDeviceWLRProtocol;
};

class CWLRDataControlManagerResource {
  public:
    CWLRDataControlManagerResource(SP<CZwlrDataControlManagerV1> resource_);

    bool                            good();

    WP<CWLRDataDevice>              device;
    std::vector<WP<CWLRDataSource>> sources;

  private:
    SP<CZwlrDataControlManagerV1> resource;
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
    std::vector<SP<CWLRDataControlManagerResource>> m_vManagers;
    std::vector<SP<CWLRDataSource>>                 m_vSources;
    std::vector<SP<CWLRDataDevice>>                 m_vDevices;
    std::vector<SP<CWLRDataOffer>>                  m_vOffers;

    //
    void setSelection(SP<CIDataSource> source, bool primary);
    void sendSelectionToDevice(SP<CWLRDataDevice> dev, SP<CIDataSource> sel, bool primary);

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
