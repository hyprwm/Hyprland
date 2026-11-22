#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/signal/Signal.hpp"

#include <string>
#include <unordered_map>
#include <vector>

class CHyprToolkitDecoration;
class IToolkitDecorationProvider;

namespace Toolkit {
    // Keeps toolkit decorations in sync with their providers and the decorations window rule,
    // renders them before frames, and tracks the pointer over them.
    class CDecorationController {
      public:
        CDecorationController();
        ~CDecorationController();

        CDecorationController(const CDecorationController&)            = delete;
        CDecorationController& operator=(const CDecorationController&) = delete;

        // false if the name is taken
        bool registerProvider(SP<IToolkitDecorationProvider> provider);
        // removes all of the provider's decorations
        void unregisterProvider(const std::string& name);

      private:
        void                                                            syncWindow(PHLWINDOW window);
        void                                                            renderFor(PHLMONITOR monitor);
        void                                                            onMouseMove(const Vector2D& coords);
        SP<CHyprToolkitDecoration>                                      decorationAt(const Vector2D& coords);
        std::vector<SP<CHyprToolkitDecoration>>                         liveDecorations();

        std::unordered_map<std::string, SP<IToolkitDecorationProvider>> m_providers;
        std::vector<WP<CHyprToolkitDecoration>>                         m_decorations;
        WP<CHyprToolkitDecoration>                                      m_hovered;

        struct {
            CHyprSignalListener updateRules;
            CHyprSignalListener render;
            CHyprSignalListener mouseMove;
        } m_listeners;
    };

    UP<CDecorationController>& decorations();
}
