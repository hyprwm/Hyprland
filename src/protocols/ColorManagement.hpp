#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "xx-color-management-v4.hpp"

class CColorManager;
class CColorManagementOutput;
class CColorManagementImageDescription;
class CColorManagementProtocol;

class CColorManager {
  public:
    CColorManager(SP<CXxColorManagerV4> resource_);

    bool good();

  private:
    SP<CXxColorManagerV4> resource;
};

class CColorManagementOutput {
  public:
    CColorManagementOutput(SP<CXxColorManagementOutputV4> resource_);
    ~CColorManagementOutput();

    bool                       good();
    wl_client*                 client();

    WP<CColorManagementOutput> self;

  private:
    SP<CXxColorManagementOutputV4>    resource;
    wl_client*                        pClient = nullptr;

    CColorManagementImageDescription* m_imageDescription = nullptr;

    friend class CColorManagementProtocol;
    friend class CColorManagementImageDescription;
};

class CColorManagementImageDescription {
  public:
    CColorManagementImageDescription(SP<CXxImageDescriptionV4> resource_, WP<CColorManagementOutput> output);

    bool       good();
    wl_client* client();

  private:
    SP<CXxImageDescriptionV4>  resource;
    wl_client*                 pClient = nullptr;

    WP<CColorManagementOutput> m_Output;

    friend class CColorManagementOutput;
};

class CColorManagementProtocol : public IWaylandProtocol {
  public:
    CColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                    destroyResource(CColorManager* resource);
    void                                    destroyResource(CColorManagementOutput* resource);

    std::vector<SP<CColorManager>>          m_vManagers;
    std::vector<SP<CColorManagementOutput>> m_vOutputs;

    friend class CColorManager;
    friend class CColorManagementOutput;
    friend class CColorManagementImageDescription;
};

namespace PROTO {
    inline UP<CColorManagementProtocol> colorManagement;
};
