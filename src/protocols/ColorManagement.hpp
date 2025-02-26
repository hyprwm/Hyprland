#pragma once

#include <drm_mode.h>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "helpers/Monitor.hpp"
#include "protocols/core/Compositor.hpp"
#include "color-management-v1.hpp"
#include "types/ColorManagement.hpp"

class CColorManager;
class CColorManagementOutput;
class CColorManagementImageDescription;
class CColorManagementProtocol;

class CColorManager {
  public:
    CColorManager(SP<CWpColorManagerV1> resource);

    bool good();

  private:
    SP<CWpColorManagerV1> m_resource;
};

class CColorManagementOutput {
  public:
    CColorManagementOutput(SP<CWpColorManagementOutputV1> resource, WP<CMonitor> monitor);

    bool                                 good();
    wl_client*                           client();

    WP<CColorManagementOutput>           self;
    WP<CColorManagementImageDescription> imageDescription;

  private:
    SP<CWpColorManagementOutputV1> m_resource;
    wl_client*                     pClient = nullptr;
    WP<CMonitor>                   m_monitor;

    friend class CColorManagementProtocol;
    friend class CColorManagementImageDescription;
};

class CColorManagementSurface {
  public:
    CColorManagementSurface(SP<CWLSurfaceResource> surface_); // temporary interface for frog CM
    CColorManagementSurface(SP<CWpColorManagementSurfaceV1> resource, SP<CWLSurfaceResource> surface_);

    bool                                       good();
    wl_client*                                 client();

    WP<CColorManagementSurface>                self;
    WP<CWLSurfaceResource>                     surface;

    const NColorManagement::SImageDescription& imageDescription();
    bool                                       hasImageDescription();
    void                                       setHasImageDescription(bool has);
    const hdr_output_metadata&                 hdrMetadata();
    void                                       setHDRMetadata(const hdr_output_metadata& metadata);
    bool                                       needsHdrMetadataUpdate();

  private:
    SP<CWpColorManagementSurfaceV1>     m_resource;
    wl_client*                          pClient = nullptr;
    NColorManagement::SImageDescription m_imageDescription;
    bool                                m_hasImageDescription = false;
    bool                                m_needsNewMetadata    = false;
    hdr_output_metadata                 m_hdrMetadata;

    friend class CXXColorManagementSurface;
    friend class CFrogColorManagementSurface;
};

class CColorManagementFeedbackSurface {
  public:
    CColorManagementFeedbackSurface(SP<CWpColorManagementSurfaceFeedbackV1> resource, SP<CWLSurfaceResource> surface_);

    bool                                good();
    wl_client*                          client();

    WP<CColorManagementFeedbackSurface> self;
    WP<CWLSurfaceResource>              surface;

  private:
    SP<CWpColorManagementSurfaceFeedbackV1> m_resource;
    wl_client*                              pClient = nullptr;

    WP<CColorManagementImageDescription>    m_currentPreferred;

    friend class CColorManagementProtocol;
};

class CColorManagementIccCreator {
  public:
    CColorManagementIccCreator(SP<CWpImageDescriptionCreatorIccV1> resource);

    bool                                good();
    wl_client*                          client();

    WP<CColorManagementIccCreator>      self;

    NColorManagement::SImageDescription settings;

  private:
    SP<CWpImageDescriptionCreatorIccV1> m_resource;
    wl_client*                          pClient = nullptr;
};

class CColorManagementParametricCreator {
  public:
    CColorManagementParametricCreator(SP<CWpImageDescriptionCreatorParamsV1> resource);

    bool                                  good();
    wl_client*                            client();

    WP<CColorManagementParametricCreator> self;

    NColorManagement::SImageDescription   settings;

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

    SP<CWpImageDescriptionCreatorParamsV1> m_resource;
    wl_client*                             pClient   = nullptr;
    uint32_t                               valuesSet = 0; // enum eValuesSet
};

class CColorManagementImageDescription {
  public:
    CColorManagementImageDescription(SP<CWpImageDescriptionV1> resource, bool allowGetInformation = false);

    bool                                 good();
    wl_client*                           client();
    SP<CWpImageDescriptionV1>            resource();

    WP<CColorManagementImageDescription> self;

    NColorManagement::SImageDescription  settings;

  private:
    SP<CWpImageDescriptionV1> m_resource;
    wl_client*                pClient               = nullptr;
    bool                      m_allowGetInformation = false;

    friend class CColorManagementOutput;
};

class CColorManagementImageDescriptionInfo {
  public:
    CColorManagementImageDescriptionInfo(SP<CWpImageDescriptionInfoV1> resource, const NColorManagement::SImageDescription& settings_);

    bool       good();
    wl_client* client();

  private:
    SP<CWpImageDescriptionInfoV1>       m_resource;
    wl_client*                          pClient = nullptr;
    NColorManagement::SImageDescription settings;
};

class CColorManagementProtocol : public IWaylandProtocol {
  public:
    CColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name, bool debug = false);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onImagePreferredChanged(uint32_t preferredId);
    void         onMonitorImageDescriptionChanged(WP<CMonitor> monitor);

  private:
    void                                               destroyResource(CColorManager* resource);
    void                                               destroyResource(CColorManagementOutput* resource);
    void                                               destroyResource(CColorManagementSurface* resource);
    void                                               destroyResource(CColorManagementFeedbackSurface* resource);
    void                                               destroyResource(CColorManagementIccCreator* resource);
    void                                               destroyResource(CColorManagementParametricCreator* resource);
    void                                               destroyResource(CColorManagementImageDescription* resource);

    std::vector<SP<CColorManager>>                     m_vManagers;
    std::vector<SP<CColorManagementOutput>>            m_vOutputs;
    std::vector<SP<CColorManagementSurface>>           m_vSurfaces;
    std::vector<SP<CColorManagementFeedbackSurface>>   m_vFeedbackSurfaces;
    std::vector<SP<CColorManagementIccCreator>>        m_vIccCreators;
    std::vector<SP<CColorManagementParametricCreator>> m_vParametricCreators;
    std::vector<SP<CColorManagementImageDescription>>  m_vImageDescriptions;
    bool                                               m_debug = false;

    friend class CColorManager;
    friend class CColorManagementOutput;
    friend class CColorManagementSurface;
    friend class CColorManagementFeedbackSurface;
    friend class CColorManagementIccCreator;
    friend class CColorManagementParametricCreator;
    friend class CColorManagementImageDescription;

    friend class CXXColorManagementSurface;
    friend class CFrogColorManagementSurface;
};

namespace PROTO {
    inline UP<CColorManagementProtocol> colorManagement;
};
