#pragma once

#include "Buffer.hpp"
#include <array>
#include <hyprutils/os/FileDescriptor.hpp>

class CDMABuffer : public IHLBuffer {
  public:
    CDMABuffer(uint32_t id, wl_client* client, const Aquamarine::SDMABUFAttrs& attrs_, std::array<Hyprutils::OS::CFileDescriptor, 4> fds);
    virtual ~CDMABuffer();

    virtual Aquamarine::eBufferCapability          caps();
    virtual Aquamarine::eBufferType                type();
    virtual bool                                   isSynchronous();
    virtual void                                   update(const CRegion& damage);
    virtual Aquamarine::SDMABUFAttrs               dmabuf();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    bool                                           good();
    void                                           closeFDs();
    std::vector<Hyprutils::OS::CFileDescriptor>    exportSyncFiles();
    bool                                           m_success = false;

  private:
    // Attribute copies are borrowed views; only m_fds owns the planes.
    Aquamarine::SDMABUFAttrs                      m_attrs;
    std::array<Hyprutils::OS::CFileDescriptor, 4> m_fds;

    struct {
        CHyprSignalListener resourceDestroy;
    } m_listeners;
};
