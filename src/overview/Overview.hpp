#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/signal/Signal.hpp"

#include <string>
#include <optional>

namespace Overview {

    struct SOverviewState {
        bool         open = false;
        PHLMONITOR   monitor;
        PHLWORKSPACE workspace;
        std::string  query;
    };

    /*
     * An interface for an overview. Normally, Hyprland only implements one,
     * but this is an easy to override point of entry for plugins.
     */
    class IOverview {
      public:
        virtual ~IOverview() = default;

        /*
         * Request to open the overview on a given monitor. The overview doesn't
         * have to be confined to it, it's merely a hint.
         */
        virtual void open(PHLMONITOR monitor) = 0;

        /*
         * Request to close the overview prematurely. This is not closed if the overview
         * decides to close itself.
         */
        virtual void         close() = 0;

        virtual bool         isOpen() const = 0;
        virtual bool         shouldRenderWorkspace(PHLWORKSPACE workspace) const;
        virtual PHLWORKSPACE inputWorkspace() const;

        struct {
            CSignalT<> opened;
            CSignalT<> closed;
        } m_events;

      protected:
        IOverview() = default;
    };

    class IOverviewGestureOpenable {
      public:
        virtual ~IOverviewGestureOpenable() = default;

        virtual bool beginOpenGesture(PHLMONITOR monitor) = 0;
        virtual void updateOpenGesture(float completion)  = 0;
        virtual void endOpenGesture(bool commit)          = 0;
    };

    class IOverviewGestureMovable {
      public:
        virtual ~IOverviewGestureMovable() = default;

        virtual bool beginMoveGesture()         = 0;
        virtual void updateMoveGesture(float Δ) = 0;
        virtual void endMoveGesture()           = 0;
    };

    class IOverviewNavigable {
      public:
        virtual ~IOverviewNavigable() = default;

        virtual bool moveLeft()  = 0;
        virtual bool moveRight() = 0;
    };

    class IOverviewQueryOpenable {
      public:
        virtual ~IOverviewQueryOpenable() = default;

        virtual void open(PHLMONITOR monitor, const std::string& query) = 0;
    };

    class IOverviewStateProvider {
      public:
        virtual ~IOverviewStateProvider() = default;

        virtual SOverviewState state() const = 0;
    };

    void                openWithQuery(IOverview* overview, PHLMONITOR monitor, const std::string& query);
    SOverviewState      state(const IOverview* overview);
    std::optional<bool> moveLeft(IOverview* overview);
    std::optional<bool> moveRight(IOverview* overview);

    /*
     * If you are a plugin and want to override this, remember to reset this ptr on unload and
     * populate with Overview::Hyprland::COverview
     */
    UP<IOverview>& overview();
};
