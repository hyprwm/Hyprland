#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include "WaylandProtocol.hpp"
#include "ext-image-capture-source-v1.hpp"

class CImageCaptureSource {
  public:
    CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource_, SP<CMonitor> monitor);
    CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource_, SP<CWindow> window);
    ~CImageCaptureSource();

    wl_resource* res();

    struct {
        CSignal destroy;
    } events;

  private:
    SP<CExtImageCaptureSourceV1> resource;

    // don't really have specific names, just anything that will invalidate source
    struct {
        CHyprSignalListener destroy1;
        CHyprSignalListener destroy2;
        CHyprSignalListener destroy3;
    } listeners;

    SP<CMonitor> monitor;
    SP<CWindow>  window;
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
    UP<COutputImageCaptureSourceProtocol>                           output;
    UP<CToplevelImageCaptureSourceProtocol>                         toplevel;

    std::vector<SP<CExtOutputImageCaptureSourceManagerV1>>          m_vOutputManagers;
    std::vector<SP<CExtForeignToplevelImageCaptureSourceManagerV1>> m_vToplevelManagers;

    std::vector<SP<CImageCaptureSource>>                            m_vSources;

    friend class COutputImageCaptureSourceProtocol;
    friend class CToplevelImageCaptureSourceProtocol;
};

namespace PROTO {
    inline UP<CImageCaptureSourceProtocol> imageCaptureSource;
};
