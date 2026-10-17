#pragma once

#include <vector>

#include "../defines.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "WaylandProtocol.hpp"
#include "ext-image-capture-source-v1.hpp"
#include "hyprland-workspace-image-capture-source-v1.hpp"

class CImageCopyCaptureSession;

namespace Screenshare {
    class CWorkspaceCaptureSource;
}

class CImageCaptureSource {
  public:
    CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource, PHLMONITOR pMonitor);
    CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource, PHLWINDOW pWindow);
    CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource, PHLWORKSPACE pWorkspace, uint32_t mode);

    bool                    good();
    std::string             getName();
    std::string             getTypeName();
    CBox                    logicalBox();

    WP<CImageCaptureSource> m_self;

  private:
    SP<CExtImageCaptureSourceV1>             m_resource;

    PHLMONITORREF                            m_monitor;
    PHLWINDOWREF                             m_window;
    SP<Screenshare::CWorkspaceCaptureSource> m_workspace;

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

class CHyprlandWorkspaceCaptureSourceProtocol : public IWaylandProtocol {
  public:
    CHyprlandWorkspaceCaptureSourceProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
};

class CImageCaptureSourceProtocol {
  public:
    CImageCaptureSourceProtocol();

    SP<CImageCaptureSource> sourceFromResource(wl_resource* resource);

    void                    destroyResource(CExtOutputImageCaptureSourceManagerV1* resource);
    void                    destroyResource(CExtForeignToplevelImageCaptureSourceManagerV1* resource);
    void                    destroyResource(CHyprlandWorkspaceImageCaptureSourceManagerV1* resource);
    void                    destroyResource(CImageCaptureSource* resource);

  private:
    UP<COutputImageCaptureSourceProtocol>                           m_output;
    UP<CToplevelImageCaptureSourceProtocol>                         m_toplevel;
    UP<CHyprlandWorkspaceCaptureSourceProtocol>                     m_workspace;

    std::vector<SP<CExtOutputImageCaptureSourceManagerV1>>          m_outputManagers;
    std::vector<SP<CExtForeignToplevelImageCaptureSourceManagerV1>> m_toplevelManagers;
    std::vector<SP<CHyprlandWorkspaceImageCaptureSourceManagerV1>>  m_workspaceManagers;

    std::vector<SP<CImageCaptureSource>>                            m_sources;

    friend class COutputImageCaptureSourceProtocol;
    friend class CToplevelImageCaptureSourceProtocol;
    friend class CHyprlandWorkspaceCaptureSourceProtocol;
};

namespace PROTO {
    inline UP<CImageCaptureSourceProtocol> imageCaptureSource;
};
