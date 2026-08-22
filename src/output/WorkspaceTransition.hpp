#pragma once

#include "../helpers/AnimatedVariable.hpp"

#include <optional>
#include <string>
#include <vector>

namespace Monitor {
    class CMonitor;

    struct SWorkspaceTransitionState {
        PHLWORKSPACEREF      workspace;
        PHLANIMVAR<Vector2D> offset;
        PHLANIMVAR<float>    alpha;
        bool                 forceRendering = false;
    };

    class CWorkspaceTransition {
      public:
        explicit CWorkspaceTransition(CMonitor& owner);

        SWorkspaceTransitionState&       ensure(PHLWORKSPACE workspace);
        SWorkspaceTransitionState*       get(PHLWORKSPACE workspace);
        const SWorkspaceTransitionState* get(PHLWORKSPACE workspace) const;

        bool                             participates(PHLWORKSPACE workspace) const;
        bool                             isAnimating(PHLWORKSPACE workspace) const;
        bool                             forceRendering(PHLWORKSPACE workspace) const;
        float                            alphaValue(PHLWORKSPACE workspace) const;
        Vector2D                         offsetValue(PHLWORKSPACE workspace) const;
        std::string                      style(PHLWORKSPACE workspace) const;
        std::vector<PHLWORKSPACE>        participants(std::optional<bool> special = std::nullopt) const;

        void                             setForceRendering(PHLWORKSPACE workspace, bool forceRendering);
        void                             transferTo(CWorkspaceTransition& destination, PHLWORKSPACE workspace);
        void                             remove(PHLWORKSPACE workspace);
        void                             clear();
        void                             prune(PHLWORKSPACE workspace = nullptr);

      private:
        void                                       installCallbacks(SWorkspaceTransitionState& state);

        CMonitor&                                  m_owner;
        std::vector<UP<SWorkspaceTransitionState>> m_states;
    };
}
