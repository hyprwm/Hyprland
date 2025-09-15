#pragma once

// TODO: implement rest of proto

#include <vector>

#include "../defines.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/Format.hpp"
#include "WaylandProtocol.hpp"
#include "ImageCaptureSource.hpp"
#include "ext-image-copy-capture-v1.hpp"

class IHLBuffer;
class CScreenshareSession;
class CScreenshareFrame;

class CImageCopyCaptureFrame {
  public:
    CImageCopyCaptureFrame(SP<CExtImageCopyCaptureFrameV1> resource, WP<CImageCopyCaptureSession> session);
    ~CImageCopyCaptureFrame();

  private:
    SP<CExtImageCopyCaptureFrameV1> m_resource;
    WP<CImageCopyCaptureSession>    m_session;
    UP<CScreenshareFrame>           m_frame;

    bool                            m_captured = false;
    SP<IHLBuffer>                   m_buffer;
    CRegion                         m_clientDamage;

    friend class CImageCopyCaptureSession;
};

class CImageCopyCaptureSession {
  public:
    CImageCopyCaptureSession(SP<CExtImageCopyCaptureSessionV1> resource, SP<CImageCaptureSource> source, extImageCopyCaptureManagerV1Options options);
    ~CImageCopyCaptureSession();

  private:
    SP<CExtImageCopyCaptureSessionV1> m_resource;

    SP<CImageCaptureSource>           m_source;
    UP<CScreenshareSession>           m_session;
    WP<CImageCopyCaptureFrame>        m_frame;

    Vector2D                          m_bufferSize;
    bool                              m_paintCursor;

    struct {
        CHyprSignalListener constraintsChanged;
        CHyprSignalListener stopped;
    } m_listeners;

    WP<CImageCopyCaptureSession> m_self;

    //
    void sendConstraints();

    friend class CImageCopyCaptureProtocol;
    friend class CImageCopyCaptureFrame;
};

class CImageCopyCaptureProtocol : public IWaylandProtocol {
  public:
    CImageCopyCaptureProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         destroyResource(CExtImageCopyCaptureManagerV1* resource);
    void         destroyResource(CImageCopyCaptureSession* resource);
    void         destroyResource(CImageCopyCaptureFrame* resource);

  private:
    std::vector<SP<CExtImageCopyCaptureManagerV1>> m_managers;
    std::vector<SP<CImageCopyCaptureSession>>      m_sessions;

    std::vector<SP<CImageCopyCaptureFrame>>        m_frames;

    friend class CImageCopyCaptureSession;
};

namespace PROTO {
    inline UP<CImageCopyCaptureProtocol> imageCopyCapture;
};
