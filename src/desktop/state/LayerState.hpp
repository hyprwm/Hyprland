#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../DesktopTypes.hpp"

#include <vector>

namespace Desktop {
    class CLayerState {
      public:
        CLayerState();
        ~CLayerState() = default;

        const std::vector<PHLLS>& layers() const;

        void                      removeSafe(PHLLS ls);
        void                      clear();

      private:
        std::vector<PHLLS> m_layers;

        struct {
            CHyprSignalListener viewCreate, viewDestroy;
        } m_listeners;
    };

    UP<CLayerState>& layerState();
};
