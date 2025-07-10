#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "wayland.hpp"
#include "linux-dmabuf-v1.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/Format.hpp"
#include "../helpers/Monitor.hpp"
#include <aquamarine/buffer/Buffer.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

class CDMABuffer;
class CWLSurfaceResource;

class CLinuxDMABuffer {
  public:
    CLinuxDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs attrs);
    ~CLinuxDMABuffer();

    bool good();

  private:
    SP<CDMABuffer> m_buffer;

    struct {
        CHyprSignalListener bufferResourceDestroy;
    } m_listeners;

    friend class CLinuxDMABUFParamsResource;
};

#pragma pack(push, 1)
struct SDMABUFFormatTableEntry {
    uint32_t fmt = 0;
    char     pad[4];
    uint64_t modifier = 0;
};
#pragma pack(pop)

struct SDMABUFTranche {
    dev_t                   device = 0;
    uint32_t                flags  = 0;
    std::vector<SDRMFormat> formats;
    std::vector<uint16_t>   indicies;
};

class CDMABUFFormatTable {
  public:
    CDMABUFFormatTable(SDMABUFTranche rendererTranche, std::vector<std::pair<PHLMONITORREF, SDMABUFTranche>> tranches);
    ~CDMABUFFormatTable() = default;

    Hyprutils::OS::CFileDescriptor                        m_tableFD;
    size_t                                                m_tableSize = 0;
    SDMABUFTranche                                        m_rendererTranche;
    std::vector<std::pair<PHLMONITORREF, SDMABUFTranche>> m_monitorTranches;
};

class CLinuxDMABUFParamsResource {
  public:
    CLinuxDMABUFParamsResource(UP<CZwpLinuxBufferParamsV1>&& resource_);
    ~CLinuxDMABUFParamsResource() = default;

    bool                         good();
    void                         create(uint32_t id); // 0 means not immed

    SP<Aquamarine::SDMABUFAttrs> m_attrs;
    WP<CLinuxDMABuffer>          m_createdBuffer;
    bool                         m_used = false;

  private:
    UP<CZwpLinuxBufferParamsV1> m_resource;

    bool                        verify();
    bool                        commence();
};

class CLinuxDMABUFFeedbackResource {
  public:
    CLinuxDMABUFFeedbackResource(UP<CZwpLinuxDmabufFeedbackV1>&& resource_, SP<CWLSurfaceResource> surface_);
    ~CLinuxDMABUFFeedbackResource() = default;

    bool                   good();
    void                   sendDefaultFeedback();
    void                   sendTranche(SDMABUFTranche& tranche);

    SP<CWLSurfaceResource> m_surface; // optional, for surface feedbacks

  private:
    UP<CZwpLinuxDmabufFeedbackV1> m_resource;
    bool                          m_lastFeedbackWasScanout = false;

    friend class CLinuxDMABufV1Protocol;
};

class CLinuxDMABUFResource {
  public:
    CLinuxDMABUFResource(UP<CZwpLinuxDmabufV1>&& resource_);
    ~CLinuxDMABUFResource() = default;

    bool good();
    void sendMods();

  private:
    UP<CZwpLinuxDmabufV1> m_resource;
};

class CLinuxDMABufV1Protocol : public IWaylandProtocol {
  public:
    CLinuxDMABufV1Protocol(const wl_interface* iface, const int& ver, const std::string& name);
    ~CLinuxDMABufV1Protocol() = default;

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
    void         updateScanoutTranche(SP<CWLSurfaceResource> surface, PHLMONITOR pMonitor);

  private:
    void destroyResource(CLinuxDMABUFResource* resource);
    void destroyResource(CLinuxDMABUFFeedbackResource* resource);
    void destroyResource(CLinuxDMABUFParamsResource* resource);
    void destroyResource(CLinuxDMABuffer* resource);

    void resetFormatTable();

    //
    std::vector<UP<CLinuxDMABUFResource>>         m_managers;
    std::vector<UP<CLinuxDMABUFFeedbackResource>> m_feedbacks;
    std::vector<UP<CLinuxDMABUFParamsResource>>   m_params;
    std::vector<UP<CLinuxDMABuffer>>              m_buffers;

    UP<CDMABUFFormatTable>                        m_formatTable;
    dev_t                                         m_mainDevice;
    Hyprutils::OS::CFileDescriptor                m_mainDeviceFD;

    friend class CLinuxDMABUFResource;
    friend class CLinuxDMABUFFeedbackResource;
    friend class CLinuxDMABUFParamsResource;
    friend class CLinuxDMABuffer;
};

namespace PROTO {
    inline UP<CLinuxDMABufV1Protocol> linuxDma;
};
