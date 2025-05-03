#pragma once

#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "../helpers/signal/Signal.hpp"
#include "cursor-shape-v1.hpp"

class CCursorShapeProtocol : public IWaylandProtocol {
  public:
    CCursorShapeProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    struct SSetShapeEvent {
        CWpCursorShapeDeviceV1*    pMgr = nullptr;
        wpCursorShapeDeviceV1Shape shape;
        std::string                shapeName;
    };

    struct {
        CSignal setShape;
    } m_events;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void onDeviceResourceDestroy(wl_resource* res);

    void onGetPointer(CWpCursorShapeManagerV1* pMgr, uint32_t id, wl_resource* pointer);
    void onGetTabletToolV2(CWpCursorShapeManagerV1* pMgr, uint32_t id, wl_resource* tablet);

    void onSetShape(CWpCursorShapeDeviceV1* pMgr, uint32_t serial, wpCursorShapeDeviceV1Shape shape);
    void createCursorShapeDevice(CWpCursorShapeManagerV1* pMgr, uint32_t id, wl_resource* resource);

    //
    std::vector<SP<CWpCursorShapeDeviceV1>>  m_devices;
    std::vector<UP<CWpCursorShapeManagerV1>> m_managers;
};

namespace PROTO {
    inline UP<CCursorShapeProtocol> cursorShape;
};