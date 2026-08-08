#pragma once

#include "../../DesktopTypes.hpp"

#include <optional>

namespace Fullscreen {
    enum eFullscreenMode : int8_t;
}

namespace Desktop::View {
    class CWindow;

    class CWindowSwallowController {
      public:
        struct SUnmapResult {
            PHLWINDOW restoredWindow;
            bool      transferredInternalFullscreen = false;
        };

        explicit CWindowSwallowController(CWindow& window);

        void         reserveCandidate();
        bool         activate();
        void         toggle();
        void         moveToWorkspace(PHLWORKSPACE workspace);
        SUnmapResult onUnmap(std::optional<Fullscreen::eFullscreenMode> internalMode = std::nullopt, bool layoutManaged = false);
        void         onDestroy();

        PHLWINDOW    swallowee() const;

      private:
        struct SRelation;

        PHLWINDOW     findCandidate() const;
        bool          activateStandalone(const SP<SRelation>& relation);
        bool          activateGroupSlot(const SP<SRelation>& relation);
        bool          toggleStandalone(const SP<SRelation>& relation);
        bool          toggleGroupSlot(const SP<SRelation>& relation);
        bool          restoreStandalone(const SP<SRelation>& relation);
        SUnmapResult  restoreAfterSwallower(const SP<SRelation>& relation, std::optional<Fullscreen::eFullscreenMode> internalMode, bool layoutManaged);
        SUnmapResult  restoreAfterSwallowee(const SP<SRelation>& relation, std::optional<Fullscreen::eFullscreenMode> internalMode, bool layoutManaged);
        void          clear(const SP<SRelation>& relation);

        CWindow&      m_window;
        SP<SRelation> m_outgoingRelation;
        SP<SRelation> m_incomingRelation;
    };
}
