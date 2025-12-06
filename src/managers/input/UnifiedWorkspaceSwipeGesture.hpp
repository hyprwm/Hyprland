#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../desktop/DesktopTypes.hpp"

class CUnifiedWorkspaceSwipeGesture {
  public:
    void begin();
    void update(double delta);
    void end();

    bool isGestureInProgress();

  private:
    PHLWORKSPACE  m_workspaceBegin = nullptr;
    PHLMONITORREF m_monitor;

    double        m_delta            = 0;
    int           m_initialDirection = 0;
    float         m_avgSpeed         = 0;
    int           m_speedPoints      = 0;
    int           m_touchID          = 0;

    friend class CWorkspaceSwipeGesture;
    friend class CInputManager;
};

inline UP<CUnifiedWorkspaceSwipeGesture> g_pUnifiedWorkspaceSwipe = makeUnique<CUnifiedWorkspaceSwipeGesture>();
