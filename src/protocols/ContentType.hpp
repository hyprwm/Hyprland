#pragma once

#include "WaylandProtocol.hpp"
#include "core/Compositor.hpp"
#include "content-type-v1.hpp"

class CContentTypeManager {
  public:
    CContentTypeManager(SP<CWpContentTypeManagerV1> resource);

    bool good();

  private:
    SP<CWpContentTypeManagerV1> m_resource;
};

class CContentType {
  public:
    CContentType(SP<CWpContentTypeV1> resource);

    bool                good();
    wl_client*          client();
    wpContentTypeV1Type value = WP_CONTENT_TYPE_V1_TYPE_NONE;

    WP<CContentType>    self;

  private:
    CContentType(WP<CWLSurfaceResource> surface);
    SP<CWpContentTypeV1> m_resource;
    wl_client*           m_pClient = nullptr;

    CHyprSignalListener  destroy;

    friend class CContentTypeProtocol;
};

class CContentTypeProtocol : public IWaylandProtocol {
  public:
    CContentTypeProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void     bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    SP<CContentType> getContentType(WP<CWLSurfaceResource> surface);

  private:
    void                                 destroyResource(CContentTypeManager* resource);
    void                                 destroyResource(CContentType* resource);

    std::vector<SP<CContentTypeManager>> m_vManagers;
    std::vector<SP<CContentType>>        m_vContentTypes;

    friend class CContentTypeManager;
    friend class CContentType;
};

namespace PROTO {
    inline UP<CContentTypeProtocol> contentType;
};
