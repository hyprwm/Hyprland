#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/signal/Signal.hpp"

namespace Overview {

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
        virtual void close() = 0;

        virtual bool isOpen() const = 0;

        struct {
            CSignalT<> opened;
            CSignalT<> closed;
        } m_events;

      protected:
        IOverview() = default;
    };

    /*
     * If you are a plugin and want to override this, remember to reset this ptr on unload and
     * populate with Overview::Hyprland::COverview
     */
    UP<IOverview>& overview();
};
