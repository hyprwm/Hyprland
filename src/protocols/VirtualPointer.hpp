#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <array>
#include "WaylandProtocol.hpp"
#include "wlr-virtual-pointer-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CVirtualPointerV1Resource {
  public:
    CVirtualPointerV1Resource(SP<CZwlrVirtualPointerV1> resource_);
    ~CVirtualPointerV1Resource();

    struct {
        CSignal destroy;
    } events;

    bool         good();
    wlr_pointer* wlr();
    wl_client*   client();

  private:
    SP<CZwlrVirtualPointerV1>             resource;
    wlr_pointer                           pointer;

    uint32_t                              axis = 0;

    std::array<wlr_pointer_axis_event, 2> axisEvents;
};

class CVirtualPointerProtocol : public IWaylandProtocol {
  public:
    CVirtualPointerProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    struct {
        CSignal newPointer; // SP<CVirtualPointerV1Resource>
    } events;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CVirtualPointerV1Resource* pointer);
    void onCreatePointer(CZwlrVirtualPointerManagerV1* pMgr, wl_resource* seat, uint32_t id);

    //
    std::vector<UP<CZwlrVirtualPointerManagerV1>> m_vManagers;
    std::vector<SP<CVirtualPointerV1Resource>>    m_vPointers;

    friend class CVirtualPointerV1Resource;
};

namespace PROTO {
    inline UP<CVirtualPointerProtocol> virtualPointer;
};
