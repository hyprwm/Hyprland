#pragma once

#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "pointer-constraints-unstable-v1.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/signal/Signal.hpp"

class CWLSurface;
class CWLSurfaceResource;

class CPointerConstraint {
  public:
    CPointerConstraint(SP<CZwpLockedPointerV1> resource_, SP<CWLSurfaceResource> surf, wl_resource* region, zwpPointerConstraintsV1Lifetime lifetime_);
    CPointerConstraint(SP<CZwpConfinedPointerV1> resource_, SP<CWLSurfaceResource> surf, wl_resource* region, zwpPointerConstraintsV1Lifetime lifetime_);
    ~CPointerConstraint();

    bool           good();

    void           deactivate();
    void           activate();
    bool           isActive();

    SP<CWLSurface> owner();

    CRegion        logicConstraintRegion();
    bool           isLocked();
    Vector2D       logicPositionHint();

  private:
    SP<CZwpLockedPointerV1>         m_resourceLocked;
    SP<CZwpConfinedPointerV1>       m_resourceConfined;

    WP<CWLSurface>                  m_hlSurface;

    CRegion                         m_region;
    bool                            m_hintSet             = false;
    Vector2D                        m_positionHint        = {-1, -1};
    Vector2D                        m_cursorPosOnActivate = {-1, -1};
    bool                            m_active              = false;
    bool                            m_locked              = false;
    bool                            m_dead                = false;
    zwpPointerConstraintsV1Lifetime m_lifetime            = ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT;

    void                            sharedConstructions();
    void                            onSetRegion(wl_resource* region);

    struct {
        CHyprSignalListener destroySurface;
    } m_listeners;
};

class CPointerConstraintsProtocol : public IWaylandProtocol {
  public:
    CPointerConstraintsProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyPointerConstraint(CPointerConstraint* constraint);
    void onLockPointer(CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region, zwpPointerConstraintsV1Lifetime lifetime);
    void onConfinePointer(CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region, zwpPointerConstraintsV1Lifetime lifetime);
    void onNewConstraint(SP<CPointerConstraint> constraint, CZwpPointerConstraintsV1* pMgr);

    //
    std::vector<UP<CZwpPointerConstraintsV1>> m_managers;
    std::vector<SP<CPointerConstraint>>       m_constraints;

    friend class CPointerConstraint;
};

namespace PROTO {
    inline UP<CPointerConstraintsProtocol> constraints;
};