#pragma once

#include "../includes.hpp"
#include "../helpers/Region.hpp"
#include "../helpers/WLListener.hpp"

class CWLSurface;

class CConstraint {
  public:
    CConstraint(wlr_pointer_constraint_v1* constraint, CWLSurface* owner);
    ~CConstraint();

    void        onCommit();
    void        onDestroy();
    void        onSetRegion();
    CRegion     logicConstraintRegion();
    bool        isLocked();
    Vector2D    logicPositionHint();

    void        deactivate();
    void        activate();
    bool        active();

    CWLSurface* owner();

  private:
    bool                       m_bActive = false;
    CWLSurface*                m_pOwner  = nullptr;
    wlr_pointer_constraint_v1* m_pConstraint;

    CRegion                    m_rRegion;
    bool                       m_bHintSet             = false;
    Vector2D                   m_vPositionHint        = {-1, -1};
    Vector2D                   m_vCursorPosOnActivate = {-1, -1};

    // for oneshot constraints that have been activated once
    bool m_bDead = false;

    DYNLISTENER(destroyConstraint);
    DYNLISTENER(setConstraintRegion);

    void initSignals();
};