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

struct SImageDescription {
    int                              iccFd   = -1;
    uint32_t                         iccSize = 0;

    xxColorManagerV4TransferFunction transferFunction      = XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB;
    float                            transferFunctionPower = 1.0f;

    bool                             primariesNameSet = false;
    xxColorManagerV4Primaries        primariesNamed   = XX_COLOR_MANAGER_V4_PRIMARIES_SRGB;
    // primaries are stored as FP values with the same scale as standard defines (0.0 - 1.0)
    // wayland protocol expects int32_t values multiplied by 10000
    // drm expects uint16_t values multiplied by 50000
    // frog protocol expects drm values
    struct SPCPRimaries {
        struct {
            float x = 0;
            float y = 0;
        } red, green, blue, white;
    } primaries, masteringPrimaries;

    // luminances in cd/m²
    // protos and drm expect min * 10000
    struct SPCLuminances {
        float    min       = 0.2; // 0.2 cd/m²
        uint32_t max       = 80;  // 80 cd/m²
        uint32_t reference = 80;  // 80 cd/m²
    } luminances;
    struct SPCMasteringLuminances {
        float    min = 0;
        uint32_t max = 0;
    } masteringLuminances;

    uint32_t maxCLL  = 0;
    uint32_t maxFALL = 0;
};

namespace NColorPrimaries {
    static const auto BT709 =
        SImageDescription::SPCPRimaries{.red = {.x = 0.64, .y = 0.33}, .green = {.x = 0.30, .y = 0.60}, .blue = {.x = 0.15, .y = 0.06}, .white = {.x = 0.3127, .y = 0.3290}};

    static const auto BT2020 =
        SImageDescription::SPCPRimaries{.red = {.x = 0.708, .y = 0.292}, .green = {.x = 0.170, .y = 0.797}, .blue = {.x = 0.131, .y = 0.046}, .white = {.x = 0.3127, .y = 0.3290}};
}

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
    CColorManagementSurface(SP<CWLSurfaceResource> surface_); // temporary interface for frog CM
    CColorManagementSurface(SP<CXxColorManagementSurfaceV4> resource_, SP<CWLSurfaceResource> surface_);

    bool                        good();
    wl_client*                  client();

    WP<CColorManagementSurface> self;
    WP<CWLSurfaceResource>      surface;

    const SImageDescription&    imageDescription();
    bool                        hasImageDescription();

  private:
    SP<CXxColorManagementSurfaceV4> resource;
    wl_client*                      pClient = nullptr;
    SImageDescription               m_imageDescription;
    bool                            m_hasImageDescription = false;

    friend class CFrogColorManagementSurface;
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

    WP<CColorManagementImageDescription>    m_currentPreferred;

    friend class CColorManagementProtocol;
};

class CColorManagementParametricCreator {
  public:
    CColorManagementParametricCreator(SP<CXxImageDescriptionCreatorParamsV4> resource_);

    bool                                  good();
    wl_client*                            client();

    WP<CColorManagementParametricCreator> self;

    SImageDescription                     settings;

  private:
    enum eValuesSet : uint32_t { // NOLINT
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
    CColorManagementImageDescription(SP<CXxImageDescriptionV4> resource_, bool allowGetInformation = false);

    bool                                 good();
    wl_client*                           client();
    SP<CXxImageDescriptionV4>            resource();

    WP<CColorManagementImageDescription> self;

    SImageDescription                    settings;

  private:
    SP<CXxImageDescriptionV4> m_resource;
    wl_client*                pClient               = nullptr;
    bool                      m_allowGetInformation = false;

    friend class CColorManagementOutput;
};

class CColorManagementImageDescriptionInfo {
  public:
    CColorManagementImageDescriptionInfo(SP<CXxImageDescriptionInfoV4> resource_, const SImageDescription& settings_);

    bool       good();
    wl_client* client();

  private:
    SP<CXxImageDescriptionInfoV4> m_resource;
    wl_client*                    pClient = nullptr;
    SImageDescription             settings;
};

class CColorManagementProtocol : public IWaylandProtocol {
  public:
    CColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onImagePreferredChanged();

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

    friend class CFrogColorManagementSurface;
};

namespace PROTO {
    inline UP<CColorManagementProtocol> colorManagement;
};
