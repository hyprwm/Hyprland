#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "primary-selection-unstable-v1.hpp"
#include "types/DataDevice.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CPrimarySelectionOffer;
class CPrimarySelectionSource;
class CPrimarySelectionDevice;
class CPrimarySelectionManager;

class CPrimarySelectionOffer {
  public:
    CPrimarySelectionOffer(SP<CZwpPrimarySelectionOfferV1> resource_, SP<CIDataSource> source_);

    bool             good();
    void             sendData();

    bool             dead = false;

    WP<CIDataSource> source;

  private:
    SP<CZwpPrimarySelectionOfferV1> resource;

    friend class CPrimarySelectionDevice;
};

class CPrimarySelectionSource : public IDataSource {
  public:
    CPrimarySelectionSource(SP<CZwpPrimarySelectionSourceV1> resource_, SP<CPrimarySelectionDevice> device_);
    ~CPrimarySelectionSource();

    static SP<CPrimarySelectionSource> fromResource(wl_resource*);

    bool                               good();

    virtual std::vector<std::string>   mimes();
    virtual void                       send(const std::string& mime, Hyprutils::OS::CFileDescriptor fd);
    virtual void                       accepted(const std::string& mime);
    virtual void                       cancelled();
    virtual void                       error(uint32_t code, const std::string& msg);

    std::vector<std::string>           mimeTypes;
    WP<CPrimarySelectionSource>        self;
    WP<CPrimarySelectionDevice>        device;

  private:
    SP<CZwpPrimarySelectionSourceV1> resource;
};

class CPrimarySelectionDevice {
  public:
    CPrimarySelectionDevice(SP<CZwpPrimarySelectionDeviceV1> resource_);

    bool                        good();
    wl_client*                  client();

    void                        sendDataOffer(SP<CPrimarySelectionOffer> offer);
    void                        sendSelection(SP<CPrimarySelectionOffer> selection);

    WP<CPrimarySelectionDevice> self;

  private:
    SP<CZwpPrimarySelectionDeviceV1> resource;
    wl_client*                       pClient = nullptr;

    friend class CPrimarySelectionProtocol;
};

class CPrimarySelectionManager {
  public:
    CPrimarySelectionManager(SP<CZwpPrimarySelectionDeviceManagerV1> resource_);

    bool                                     good();

    WP<CPrimarySelectionDevice>              device;
    std::vector<WP<CPrimarySelectionSource>> sources;

  private:
    SP<CZwpPrimarySelectionDeviceManagerV1> resource;
};

class CPrimarySelectionProtocol : public IWaylandProtocol {
  public:
    CPrimarySelectionProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CPrimarySelectionManager* resource);
    void destroyResource(CPrimarySelectionDevice* resource);
    void destroyResource(CPrimarySelectionSource* resource);
    void destroyResource(CPrimarySelectionOffer* resource);

    //
    std::vector<SP<CPrimarySelectionManager>> m_vManagers;
    std::vector<SP<CPrimarySelectionDevice>>  m_vDevices;
    std::vector<SP<CPrimarySelectionSource>>  m_vSources;
    std::vector<SP<CPrimarySelectionOffer>>   m_vOffers;

    //
    void setSelection(SP<CIDataSource> source);
    void sendSelectionToDevice(SP<CPrimarySelectionDevice> dev, SP<CIDataSource> sel);
    void updateSelection();
    void onPointerFocus();

    //
    SP<CPrimarySelectionDevice> dataDeviceForClient(wl_client*);

    friend class CPrimarySelectionManager;
    friend class CPrimarySelectionDevice;
    friend class CPrimarySelectionSource;
    friend class CPrimarySelectionOffer;
    friend class CSeatManager;

    struct {
        CHyprSignalListener onPointerFocusChange;
        m_m_listeners;
    };

    namespace PROTO {
        inline UP<CPrimarySelectionProtocol> primarySelection;
    };
