#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/signal/Signal.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <optional>
#include <vector>

class CUnifiedWorkspaceSwipeGesture {
  public:
    void     begin();
    bool     begin(PHLMONITOR monitor);
    void     update(double delta);
    void     end();
    void     cancel();

    bool     isGestureInProgress();
    uint64_t sessionID() const;

  private:
    struct SParticipant {
        PHLWORKSPACEREF                          workspace;
        Hyprutils::Math::Vector2D                offset;
        float                                    alpha  = 1.F;
        bool                                     forced = false;
        std::optional<Hyprutils::Math::Vector2D> previewOffset;
        std::optional<float>                     previewAlpha;
    };

    SParticipant&             acquire(PHLWORKSPACE workspace);
    void                      setPreviewOffset(PHLWORKSPACE workspace, const Hyprutils::Math::Vector2D& offset);
    void                      setPreviewAlpha(PHLWORKSPACE workspace, float alpha);
    void                      restore(const SParticipant& participant);
    void                      release(PHLWORKSPACE workspace);
    void                      releaseOtherParticipants(PHLWORKSPACE workspace);
    void                      finishParticipants(PHLWORKSPACE animatedNeighbor = nullptr);
    void                      restoreLayers();
    void                      reset();
    void                      resetListeners();
    bool                      validOrigin() const;
    bool                      endSegment();

    std::vector<SParticipant> m_participants;
    PHLWORKSPACE              m_workspaceBegin = nullptr;
    PHLMONITORREF             m_monitor;

    double                    m_delta            = 0;
    int                       m_initialDirection = 0;
    float                     m_avgSpeed         = 0;
    int                       m_speedPoints      = 0;
    uint64_t                  m_sessionID        = 0;
    uint64_t                  m_sessionSerial    = 0;

    struct {
        CHyprSignalListener monitorDisconnect;
        CHyprSignalListener monitorDestroy;
        CHyprSignalListener workspaceMonitorChanged;
        CHyprSignalListener workspaceActiveChanged;
    } m_listeners;

    friend class CWorkspaceSwipeGesture;
    friend class CInputManager;
};

inline UP<CUnifiedWorkspaceSwipeGesture> g_pUnifiedWorkspaceSwipe = makeUnique<CUnifiedWorkspaceSwipeGesture>();
