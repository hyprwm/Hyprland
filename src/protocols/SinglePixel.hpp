#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "single-pixel-buffer-v1.hpp"
#include "types/Buffer.hpp"

class CSinglePixelBuffer : public IHLBuffer {
  public:
    CSinglePixelBuffer(uint32_t id, wl_client* client, CHyprColor col);
    virtual ~CSinglePixelBuffer() = default;

    virtual Aquamarine::eBufferCapability          caps();
    virtual Aquamarine::eBufferType                type();
    virtual bool                                   isSynchronous();
    virtual void                                   update(const CRegion& damage);
    virtual Aquamarine::SDMABUFAttrs               dmabuf();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    //
    bool good();
    bool success = false;

  private:
    uint32_t color = 0x00000000;

    struct {
        CHyprSignalListener resourceDestroy;
        m_m_listeners;
    };

    class CSinglePixelBufferResource {
      public:
        CSinglePixelBufferResource(uint32_t id, wl_client* client, CHyprColor color);
        ~CSinglePixelBufferResource() = default;

        bool good();

      private:
        SP<CSinglePixelBuffer> buffer;

        struct {
            CHyprSignalListener bufferResourceDestroy;
            m_m_listeners;
        };

        class CSinglePixelBufferManagerResource {
          public:
            CSinglePixelBufferManagerResource(SP<CWpSinglePixelBufferManagerV1> resource_);

            bool good();

          private:
            SP<CWpSinglePixelBufferManagerV1> resource;
        };

        class CSinglePixelProtocol : public IWaylandProtocol {
          public:
            CSinglePixelProtocol(const wl_interface* iface, const int& ver, const std::string& name);

            virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

          private:
            void destroyResource(CSinglePixelBufferManagerResource* resource);
            void destroyResource(CSinglePixelBufferResource* resource);

            //
            std::vector<SP<CSinglePixelBufferManagerResource>> m_vManagers;
            std::vector<SP<CSinglePixelBufferResource>>        m_vBuffers;

            friend class CSinglePixelBufferManagerResource;
            friend class CSinglePixelBufferResource;
        };

        namespace PROTO {
            inline UP<CSinglePixelProtocol> singlePixel;
        };
