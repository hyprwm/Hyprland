#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "ext-data-control-v1.hpp"
#include "types/DataDevice.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CExtDataControlManagerResource;
class CExtDataSource;
class CExtDataDevice;
class CExtDataOffer;

class CExtDataOffer {
  public:
    CExtDataOffer(SP<CExtDataControlOfferV1> resource_, SP<IDataSource> source);

    bool            good();
    void            sendData();

    bool            m_dead    = false;
    bool            m_primary = false;

    WP<IDataSource> m_source;

  private:
    SP<CExtDataControlOfferV1> m_resource;

    friend class CExtDataDevice;
};

class CExtDataSource : public IDataSource {
  public:
    CExtDataSource(SP<CExtDataControlSourceV1> resource_, SP<CExtDataDevice> device_);
    ~CExtDataSource();
    static SP<CExtDataSource>        fromResource(wl_resource*);

    bool                             good();

    virtual std::vector<std::string> mimes();
    virtual void                     send(const std::string& mime, Hyprutils::OS::CFileDescriptor fd);
    virtual void                     accepted(const std::string& mime);
    virtual void                     cancelled();
    virtual void                     error(uint32_t code, const std::string& msg);

    std::vector<std::string>         m_mimeTypes;
    WP<CExtDataSource>               m_self;
    WP<CExtDataDevice>               m_device;

  private:
    SP<CExtDataControlSourceV1> m_resource;
};

class CExtDataDevice {
  public:
    CExtDataDevice(SP<CExtDataControlDeviceV1> resource_);

    bool               good();
    wl_client*         client();
    void               sendInitialSelections();

    void               sendDataOffer(SP<CExtDataOffer> offer);
    void               sendSelection(SP<CExtDataOffer> selection);
    void               sendPrimarySelection(SP<CExtDataOffer> selection);

    WP<CExtDataDevice> self;

  private:
    SP<CExtDataControlDeviceV1> m_resource;
    wl_client*                  m_client = nullptr;

    friend class CExtDataDeviceProtocol;
};

class CExtDataControlManagerResource {
  public:
    CExtDataControlManagerResource(SP<CExtDataControlManagerV1> resource_);

    bool                            good();

    WP<CExtDataDevice>              m_device;
    std::vector<WP<CExtDataSource>> m_sources;

  private:
    SP<CExtDataControlManagerV1> m_resource;
};

class CExtDataDeviceProtocol : public IWaylandProtocol {
  public:
    CExtDataDeviceProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CExtDataControlManagerResource* resource);
    void destroyResource(CExtDataSource* resource);
    void destroyResource(CExtDataDevice* resource);
    void destroyResource(CExtDataOffer* resource);

    //
    std::vector<SP<CExtDataControlManagerResource>> m_managers;
    std::vector<SP<CExtDataSource>>                 m_sources;
    std::vector<SP<CExtDataDevice>>                 m_devices;
    std::vector<SP<CExtDataOffer>>                  m_offers;

    //
    void setSelection(SP<IDataSource> source, bool primary);
    void sendSelectionToDevice(SP<CExtDataDevice> dev, SP<IDataSource> sel, bool primary);

    //
    SP<CExtDataDevice> dataDeviceForClient(wl_client*);

    friend class CSeatManager;
    friend class CExtDataControlManagerResource;
    friend class CExtDataSource;
    friend class CExtDataDevice;
    friend class CExtDataOffer;
};

namespace PROTO {
    inline UP<CExtDataDeviceProtocol> extDataDevice;
};
