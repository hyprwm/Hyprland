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

class CDMABuffer;
class CWLSurfaceResource;

class CLinuxDMABuffer {
  public:
    CLinuxDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs attrs);
    ~CLinuxDMABuffer();

    bool good();

  private:
    SP<CDMABuffer> buffer;

    struct {
        CHyprSignalListener bufferResourceDestroy;
    } listeners;

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
    ~CDMABUFFormatTable();

    int                                                   tableFD   = -1;
    size_t                                                tableSize = 0;
    SDMABUFTranche                                        rendererTranche;
    std::vector<std::pair<PHLMONITORREF, SDMABUFTranche>> monitorTranches;
};

class CLinuxDMABUFParamsResource {
  public:
    CLinuxDMABUFParamsResource(SP<CZwpLinuxBufferParamsV1> resource_);
    ~CLinuxDMABUFParamsResource() = default;

    bool                         good();
    void                         create(uint32_t id); // 0 means not immed

    SP<Aquamarine::SDMABUFAttrs> attrs;
    WP<CLinuxDMABuffer>          createdBuffer;
    bool                         used = false;

  private:
    SP<CZwpLinuxBufferParamsV1> resource;

    bool                        verify();
    bool                        commence();
};

class CLinuxDMABUFFeedbackResource {
  public:
    CLinuxDMABUFFeedbackResource(SP<CZwpLinuxDmabufFeedbackV1> resource_, SP<CWLSurfaceResource> surface_);
    ~CLinuxDMABUFFeedbackResource() = default;

    bool                   good();
    void                   sendDefaultFeedback();
    void                   sendTranche(SDMABUFTranche& tranche);

    SP<CWLSurfaceResource> surface; // optional, for surface feedbacks

  private:
    SP<CZwpLinuxDmabufFeedbackV1> resource;
    bool                          lastFeedbackWasScanout = false;

    friend class CLinuxDMABufV1Protocol;
};

class CLinuxDMABUFResource {
  public:
    CLinuxDMABUFResource(SP<CZwpLinuxDmabufV1> resource_);

    bool good();
    void sendMods();

  private:
    SP<CZwpLinuxDmabufV1> resource;
};

class CLinuxDMABufV1Protocol : public IWaylandProtocol {
  public:
    CLinuxDMABufV1Protocol(const wl_interface* iface, const int& ver, const std::string& name);
    ~CLinuxDMABufV1Protocol();

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
    void         updateScanoutTranche(SP<CWLSurfaceResource> surface, PHLMONITOR pMonitor);

  private:
    void destroyResource(CLinuxDMABUFResource* resource);
    void destroyResource(CLinuxDMABUFFeedbackResource* resource);
    void destroyResource(CLinuxDMABUFParamsResource* resource);
    void destroyResource(CLinuxDMABuffer* resource);

    void resetFormatTable();

    //
    std::vector<SP<CLinuxDMABUFResource>>         m_vManagers;
    std::vector<SP<CLinuxDMABUFFeedbackResource>> m_vFeedbacks;
    std::vector<SP<CLinuxDMABUFParamsResource>>   m_vParams;
    std::vector<SP<CLinuxDMABuffer>>              m_vBuffers;

    UP<CDMABUFFormatTable>                        formatTable;
    dev_t                                         mainDevice;
    int                                           mainDeviceFD = -1;

    friend class CLinuxDMABUFResource;
    friend class CLinuxDMABUFFeedbackResource;
    friend class CLinuxDMABUFParamsResource;
    friend class CLinuxDMABuffer;
};

namespace PROTO {
    inline UP<CLinuxDMABufV1Protocol> linuxDma;
};
