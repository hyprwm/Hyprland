#pragma once

#include <drm_mode.h>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "core/Compositor.hpp"
#include "xx-color-management-v4.hpp"
#include "types/ColorManagement.hpp"

class CXXColorManager;
class CXXColorManagementOutput;
class CXXColorManagementImageDescription;
class CXXColorManagementProtocol;

class CXXColorManager {
  public:
    CXXColorManager(SP<CXxColorManagerV4> resource_);

    bool good();

  private:
    SP<CXxColorManagerV4> resource;
};

class CXXColorManagementOutput {
  public:
    CXXColorManagementOutput(SP<CXxColorManagementOutputV4> resource_);

    bool                                   good();
    wl_client*                             client();

    WP<CXXColorManagementOutput>           self;
    WP<CXXColorManagementImageDescription> imageDescription;

  private:
    SP<CXxColorManagementOutputV4> resource;
    wl_client*                     pClient = nullptr;

    friend class CXXColorManagementProtocol;
    friend class CXXColorManagementImageDescription;
};

class CXXColorManagementSurface {
  public:
    CXXColorManagementSurface(SP<CWLSurfaceResource> surface_); // temporary interface for frog CM
    CXXColorManagementSurface(SP<CXxColorManagementSurfaceV4> resource_, SP<CWLSurfaceResource> surface_);

    bool                                       good();
    wl_client*                                 client();

    WP<CXXColorManagementSurface>              self;
    WP<CWLSurfaceResource>                     surface;

    const NColorManagement::SImageDescription& imageDescription();
    bool                                       hasImageDescription();
    void                                       setHasImageDescription(bool has);
    const hdr_output_metadata&                 hdrMetadata();
    void                                       setHDRMetadata(const hdr_output_metadata& metadata);
    bool                                       needsHdrMetadataUpdate();

  private:
    SP<CXxColorManagementSurfaceV4>     resource;
    wl_client*                          pClient = nullptr;
    NColorManagement::SImageDescription m_imageDescription;
    bool                                m_hasImageDescription = false;
    bool                                m_needsNewMetadata    = false;
    hdr_output_metadata                 m_hdrMetadata;

    friend class CFrogColorManagementSurface;
};

class CXXColorManagementFeedbackSurface {
  public:
    CXXColorManagementFeedbackSurface(SP<CXxColorManagementFeedbackSurfaceV4> resource_, SP<CWLSurfaceResource> surface_);

    bool                                  good();
    wl_client*                            client();

    WP<CXXColorManagementFeedbackSurface> self;
    WP<CWLSurfaceResource>                surface;

  private:
    SP<CXxColorManagementFeedbackSurfaceV4> resource;
    wl_client*                              pClient = nullptr;

    WP<CXXColorManagementImageDescription>  m_currentPreferred;

    friend class CXXColorManagementProtocol;
};

class CXXColorManagementParametricCreator {
  public:
    CXXColorManagementParametricCreator(SP<CXxImageDescriptionCreatorParamsV4> resource_);

    bool                                    good();
    wl_client*                              client();

    WP<CXXColorManagementParametricCreator> self;

    NColorManagement::SImageDescription     settings;

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

class CXXColorManagementImageDescription {
  public:
    CXXColorManagementImageDescription(SP<CXxImageDescriptionV4> resource_, bool allowGetInformation = false);

    bool                                   good();
    wl_client*                             client();
    SP<CXxImageDescriptionV4>              resource();

    WP<CXXColorManagementImageDescription> self;

    NColorManagement::SImageDescription    settings;

  private:
    SP<CXxImageDescriptionV4> m_resource;
    wl_client*                pClient               = nullptr;
    bool                      m_allowGetInformation = false;

    friend class CXXColorManagementOutput;
};

class CXXColorManagementImageDescriptionInfo {
  public:
    CXXColorManagementImageDescriptionInfo(SP<CXxImageDescriptionInfoV4> resource_, const NColorManagement::SImageDescription& settings_);

    bool       good();
    wl_client* client();

  private:
    SP<CXxImageDescriptionInfoV4>       m_resource;
    wl_client*                          pClient = nullptr;
    NColorManagement::SImageDescription settings;
};

class CXXColorManagementProtocol : public IWaylandProtocol {
  public:
    CXXColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onImagePreferredChanged();

  private:
    void                                                 destroyResource(CXXColorManager* resource);
    void                                                 destroyResource(CXXColorManagementOutput* resource);
    void                                                 destroyResource(CXXColorManagementSurface* resource);
    void                                                 destroyResource(CXXColorManagementFeedbackSurface* resource);
    void                                                 destroyResource(CXXColorManagementParametricCreator* resource);
    void                                                 destroyResource(CXXColorManagementImageDescription* resource);

    std::vector<SP<CXXColorManager>>                     m_vManagers;
    std::vector<SP<CXXColorManagementOutput>>            m_vOutputs;
    std::vector<SP<CXXColorManagementSurface>>           m_vSurfaces;
    std::vector<SP<CXXColorManagementFeedbackSurface>>   m_vFeedbackSurfaces;
    std::vector<SP<CXXColorManagementParametricCreator>> m_vParametricCreators;
    std::vector<SP<CXXColorManagementImageDescription>>  m_vImageDescriptions;

    friend class CXXColorManager;
    friend class CXXColorManagementOutput;
    friend class CXXColorManagementSurface;
    friend class CXXColorManagementFeedbackSurface;
    friend class CXXColorManagementParametricCreator;
    friend class CXXColorManagementImageDescription;

    friend class CFrogColorManagementSurface;
};

namespace PROTO {
    inline UP<CXXColorManagementProtocol> xxColorManagement;
};
