#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/signal/Signal.hpp"

#include <vector>

namespace Desktop::View {
    class IView;
}

namespace Desktop {
    class COtherViewState {
      public:
        COtherViewState();
        ~COtherViewState() = default;

        const std::vector<WP<View::IView>>& views() const;

        void                                clear();

      private:
        std::vector<WP<View::IView>> m_views;

        struct {
            CHyprSignalListener viewCreate, viewDestroy;
        } m_listeners;
    };

    UP<COtherViewState>& otherViewState();
};
