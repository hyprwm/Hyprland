#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "relative-pointer-unstable-v1.hpp"
#include "../helpers/math/Math.hpp"

class CRelativePointer {
  public:
    CRelativePointer(SP<CZwpRelativePointerV1> resource_);

    void       sendRelativeMotion(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel);

    bool       good();
    wl_client* client();

  private:
    SP<CZwpRelativePointerV1> resource;
    wl_client*                pClient = nullptr;
};

class CRelativePointerProtocol : public IWaylandProtocol {
  public:
    CRelativePointerProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         sendRelativeMotion(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyRelativePointer(CRelativePointer* pointer);
    void onGetRelativePointer(CZwpRelativePointerManagerV1* pMgr, uint32_t id, wl_resource* pointer);

    //
    std::vector<UP<CZwpRelativePointerManagerV1>> m_vManagers;
    std::vector<UP<CRelativePointer>>             m_vRelativePointers;

    friend class CRelativePointer;
};

namespace PROTO {
    inline UP<CRelativePointerProtocol> relativePointer;
};