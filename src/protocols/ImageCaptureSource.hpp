#pragma once

#include <vector>

#include "../defines.hpp"
#include "../helpers/signal/Signal.hpp"
#include "WaylandProtocol.hpp"
#include "ext-image-capture-source-v1.hpp"

class CImageCopyCaptureSession;

class CImageCaptureSource {
  public:
    CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource, PHLMONITOR pMonitor);
    CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource, PHLWINDOW pWindow);

    std::string             getName();
    std::string             getTypeName();
    CBox                    logicalBox();

    WP<CImageCaptureSource> m_self;

  private:
    SP<CExtImageCaptureSourceV1> m_resource;

    PHLMONITORREF                m_monitor;
    PHLWINDOWREF                 m_window;

    friend class CImageCopyCaptureSession;
    friend class CImageCopyCaptureCursorSession;
};

class COutputImageCaptureSourceProtocol : public IWaylandProtocol {
  public:
    COutputImageCaptureSourceProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
};

class CToplevelImageCaptureSourceProtocol : public IWaylandProtocol {
  public:
    CToplevelImageCaptureSourceProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
};

class CImageCaptureSourceProtocol {
  public:
    CImageCaptureSourceProtocol();

    SP<CImageCaptureSource> sourceFromResource(wl_resource* resource);

    void                    destroyResource(CExtOutputImageCaptureSourceManagerV1* resource);
    void                    destroyResource(CExtForeignToplevelImageCaptureSourceManagerV1* resource);
    void                    destroyResource(CImageCaptureSource* resource);

  private:
    UP<COutputImageCaptureSourceProtocol>                           m_output;
    UP<CToplevelImageCaptureSourceProtocol>                         m_toplevel;

    std::vector<SP<CExtOutputImageCaptureSourceManagerV1>>          m_outputManagers;
    std::vector<SP<CExtForeignToplevelImageCaptureSourceManagerV1>> m_toplevelManagers;

    std::vector<SP<CImageCaptureSource>>                            m_sources;

    friend class COutputImageCaptureSourceProtocol;
    friend class CToplevelImageCaptureSourceProtocol;
};

namespace PROTO {
    inline UP<CImageCaptureSourceProtocol> imageCaptureSource;
};
