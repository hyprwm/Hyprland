#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../desktop/DesktopTypes.hpp"

#include <vector>

class CUnifiedWorkspaceSwipeGesture {
  public:
    void begin();
    void update(double delta);
    void end();

    bool isGestureInProgress();

  private:
    void                         setForceRendering(PHLWORKSPACE workspace, bool force);
    void                         clearForcedWorkspaces();
    void                         cancel();

    PHLWORKSPACE                 m_workspaceBegin = nullptr;
    PHLMONITORREF                m_monitor;
    std::vector<PHLWORKSPACEREF> m_forcedWorkspaces;

    double                       m_delta            = 0;
    int                          m_initialDirection = 0;
    float                        m_avgSpeed         = 0;
    int                          m_speedPoints      = 0;
    int                          m_touchID          = 0;

    friend class CWorkspaceSwipeGesture;
    friend class CInputManager;
};

inline UP<CUnifiedWorkspaceSwipeGesture> g_pUnifiedWorkspaceSwipe = makeUnique<CUnifiedWorkspaceSwipeGesture>();
