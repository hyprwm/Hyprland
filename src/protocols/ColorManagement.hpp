#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "protocols/core/Compositor.hpp"
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

    bool                                 good();
    wl_client*                           client();

    WP<CColorManagementOutput>           self;
    WP<CColorManagementImageDescription> imageDescription;

  private:
    SP<CXxColorManagementOutputV4> resource;
    wl_client*                     pClient = nullptr;

    friend class CColorManagementProtocol;
    friend class CColorManagementImageDescription;
};

class CColorManagementSurface {
  public:
    CColorManagementSurface(SP<CXxColorManagementSurfaceV4> resource_, SP<CWLSurfaceResource> surface_);

    bool                        good();
    wl_client*                  client();

    WP<CColorManagementSurface> self;
    WP<CWLSurfaceResource>      surface;

  private:
    SP<CXxColorManagementSurfaceV4> resource;
    wl_client*                      pClient = nullptr;
};

class CColorManagementFeedbackSurface {
  public:
    CColorManagementFeedbackSurface(SP<CXxColorManagementFeedbackSurfaceV4> resource_, SP<CWLSurfaceResource> surface_);

    bool                                good();
    wl_client*                          client();

    WP<CColorManagementFeedbackSurface> self;
    WP<CWLSurfaceResource>              surface;

  private:
    SP<CXxColorManagementFeedbackSurfaceV4> resource;
    wl_client*                              pClient = nullptr;
};

struct SImageDescription {
    bool                             drmCoded              = false; // values are ready to pass to hdr_output_metadata as is
    xxColorManagerV4TransferFunction transferFunction      = XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB;
    float                            transferFunctionPower = 1.0f;
    struct SPCPRimaries {
        struct {
            uint32_t x = 0;
            uint32_t y = 0;
        } red, green, blue, white;
    } primaries, masteringPrimaries;
    struct SPCLuminances {
        uint32_t min       = 2000; // 0.2 cd/m²
        uint32_t max       = 80;   // 80 cd/m²
        uint32_t reference = 80;   // 80 cd/m²
    } luminances;
    struct SPCMasteringLuminances {
        uint32_t min = 0;
        uint32_t max = 0;
    } masteringLuminances;
    uint32_t maxCLL  = 0;
    uint32_t maxFALL = 0;
};

class CColorManagementParametricCreator {
  public:
    CColorManagementParametricCreator(SP<CXxImageDescriptionCreatorParamsV4> resource_);

    bool                                  good();
    wl_client*                            client();

    WP<CColorManagementParametricCreator> self;

    SImageDescription                     settings;

  private:
    enum eValuesSet : uint32_t {
        PC_TF                   = (1 << 0),
        PC_TF_POWER             = (1 << 1),
        PC_PRIMARIES            = (1 << 2),
        PC_LUMINANCES           = (1 << 3),
        PC_MASTERING_PRIMARIES  = (1 << 4),
        PC_MASTERING_LUMINANCES = (1 << 5),
        PC_CLL                  = (1 << 6),
        PC_FALL                 = (1 << 7),
    };

    SP<CXxImageDescriptionCreatorParamsV4> resource;
    wl_client*                             pClient   = nullptr;
    uint32_t                               valuesSet = 0; // enum eValuesSet
};

class CColorManagementImageDescription {
  public:
    CColorManagementImageDescription(SP<CXxImageDescriptionV4> resource_);

    bool                                 good();
    wl_client*                           client();
    SP<CXxImageDescriptionV4>            resource();

    WP<CColorManagementImageDescription> self;

    SImageDescription                    settings;

  private:
    SP<CXxImageDescriptionV4> m_resource;
    wl_client*                pClient = nullptr;

    friend class CColorManagementOutput;
};

class CColorManagementProtocol : public IWaylandProtocol {
  public:
    CColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                               destroyResource(CColorManager* resource);
    void                                               destroyResource(CColorManagementOutput* resource);
    void                                               destroyResource(CColorManagementSurface* resource);
    void                                               destroyResource(CColorManagementFeedbackSurface* resource);
    void                                               destroyResource(CColorManagementParametricCreator* resource);
    void                                               destroyResource(CColorManagementImageDescription* resource);

    std::vector<SP<CColorManager>>                     m_vManagers;
    std::vector<SP<CColorManagementOutput>>            m_vOutputs;
    std::vector<SP<CColorManagementSurface>>           m_vSurfaces;
    std::vector<SP<CColorManagementFeedbackSurface>>   m_vFeedbackSurfaces;
    std::vector<SP<CColorManagementParametricCreator>> m_vParametricCreators;
    std::vector<SP<CColorManagementImageDescription>>  m_vImageDescriptions;

    friend class CColorManager;
    friend class CColorManagementOutput;
    friend class CColorManagementSurface;
    friend class CColorManagementFeedbackSurface;
    friend class CColorManagementParametricCreator;
    friend class CColorManagementImageDescription;
};

namespace PROTO {
    inline UP<CColorManagementProtocol> colorManagement;
};
