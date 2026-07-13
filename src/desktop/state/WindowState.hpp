#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../DesktopTypes.hpp"
#include "WindowQuery.hpp"

#include <vector>

namespace Desktop {
    class CWindowState {
      public:
        CWindowState();
        ~CWindowState() = default;

        const std::vector<PHLWINDOW>& windows() const;

        CWindowQuery                  query() const;
        void                          raise(PHLWINDOW w);
        void                          lower(PHLWINDOW w);
        void                          clear();

        // kept for compat with old code, should be removed ASAP
        // once we remove the god-awful fadeout logic
        void removeSafe(PHLWINDOW w);

      private:
        std::vector<PHLWINDOW> m_windows;

        void                   moveToTop(PHLWINDOW w);
        void                   moveToBottom(PHLWINDOW w);
        void                   moveToZ(PHLWINDOW w, bool top);

        struct {
            CHyprSignalListener viewCreate, viewDestroy;
        } m_listeners;
    };

    UP<CWindowState>& windowState();
};
