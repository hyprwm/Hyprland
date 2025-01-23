#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "pointer-gestures-unstable-v1.hpp"
#include "../helpers/math/Math.hpp"

class CPointerGestureSwipe {
  public:
    CPointerGestureSwipe(SP<CZwpPointerGestureSwipeV1> resource_);

    bool good();

  private:
    SP<CZwpPointerGestureSwipeV1> resource;

    friend class CPointerGesturesProtocol;
};

class CPointerGesturePinch {
  public:
    CPointerGesturePinch(SP<CZwpPointerGesturePinchV1> resource_);

    bool good();

  private:
    SP<CZwpPointerGesturePinchV1> resource;

    friend class CPointerGesturesProtocol;
};

class CPointerGestureHold {
  public:
    CPointerGestureHold(SP<CZwpPointerGestureHoldV1> resource_);

    bool good();

  private:
    SP<CZwpPointerGestureHoldV1> resource;

    friend class CPointerGesturesProtocol;
};

class CPointerGesturesProtocol : public IWaylandProtocol {
  public:
    CPointerGesturesProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         swipeBegin(uint32_t timeMs, uint32_t fingers);
    void         swipeUpdate(uint32_t timeMs, const Vector2D& delta);
    void         swipeEnd(uint32_t timeMs, bool cancelled);

    void         pinchBegin(uint32_t timeMs, uint32_t fingers);
    void         pinchUpdate(uint32_t timeMs, const Vector2D& delta, double scale, double rotation);
    void         pinchEnd(uint32_t timeMs, bool cancelled);

    void         holdBegin(uint32_t timeMs, uint32_t fingers);
    void         holdEnd(uint32_t timeMs, bool cancelled);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void onGestureDestroy(CPointerGestureSwipe* gesture);
    void onGestureDestroy(CPointerGesturePinch* gesture);
    void onGestureDestroy(CPointerGestureHold* gesture);
    void onGetPinchGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer);
    void onGetSwipeGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer);
    void onGetHoldGesture(CZwpPointerGesturesV1* pMgr, uint32_t id, wl_resource* pointer);

    //
    std::vector<UP<CZwpPointerGesturesV1>> m_vManagers;
    std::vector<UP<CPointerGestureSwipe>>  m_vSwipes;
    std::vector<UP<CPointerGesturePinch>>  m_vPinches;
    std::vector<UP<CPointerGestureHold>>   m_vHolds;

    friend class CPointerGestureHold;
    friend class CPointerGesturePinch;
    friend class CPointerGestureSwipe;
};

namespace PROTO {
    inline UP<CPointerGesturesProtocol> pointerGestures;
};