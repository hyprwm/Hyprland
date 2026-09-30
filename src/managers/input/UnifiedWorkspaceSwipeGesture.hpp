#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../desktop/DesktopTypes.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <vector>

class CUnifiedWorkspaceSwipeGesture {
  public:
    void begin();
    void update(double delta);
    void end();
    void cancel();

    bool isGestureInProgress();

  private:
    struct SParticipant {
        PHLWORKSPACEREF           workspace;
        Hyprutils::Math::Vector2D offset;
        float                     alpha  = 1.F;
        bool                      forced = false;
    };

    void                      acquire(PHLWORKSPACE workspace);
    void                      restore(const SParticipant& participant);
    void                      release(PHLWORKSPACE workspace);
    void                      releaseOtherParticipants(PHLWORKSPACE workspace);
    void                      finishParticipants(PHLWORKSPACE animatedNeighbor = nullptr);
    void                      restoreLayers();
    void                      reset();

    std::vector<SParticipant> m_participants;
    PHLWORKSPACE              m_workspaceBegin = nullptr;
    PHLMONITORREF             m_monitor;

    double                    m_delta            = 0;
    int                       m_initialDirection = 0;
    float                     m_avgSpeed         = 0;
    int                       m_speedPoints      = 0;
    int                       m_touchID          = 0;

    friend class CWorkspaceSwipeGesture;
    friend class CInputManager;
};

inline UP<CUnifiedWorkspaceSwipeGesture> g_pUnifiedWorkspaceSwipe = makeUnique<CUnifiedWorkspaceSwipeGesture>();
