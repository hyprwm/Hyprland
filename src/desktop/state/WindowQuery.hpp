#pragma once

#include "../DesktopTypes.hpp"
#include "../../helpers/math/Direction.hpp"
#include "../../helpers/math/Math.hpp"

#include <optional>

namespace Desktop {
    class CWindowState;

    struct SWindowDirectionQuery {
        CBox             origin             = {};
        PHLWORKSPACE     workspace          = nullptr;
        Math::eDirection direction          = Math::DIRECTION_DEFAULT;
        bool             floatingPreference = false;
        PHLWINDOW        ignoreWindow       = nullptr;
        bool             useVectorAngles    = false;
    };

    struct SWindowCycleOptions {
        bool                focusableOnly          = false;
        std::optional<bool> floating               = std::nullopt;
        bool                visible                = false;
        bool                previous               = false;
        bool                allowFullscreenBlocked = false;
    };

    class CWindowQuery {
      public:
        CWindowQuery(const CWindowState& state);
        ~CWindowQuery() = default;

        PHLWINDOW inDirection(PHLWINDOW window, Math::eDirection direction) const;
        PHLWINDOW inDirection(const SWindowDirectionQuery& query) const;

        PHLWINDOW cycle(PHLWINDOW current, const SWindowCycleOptions& options = {}) const;
        PHLWINDOW cycleHistory(PHLWINDOWREF current, const SWindowCycleOptions& options = {}) const;

      private:
        const CWindowState& m_state;
    };
}
