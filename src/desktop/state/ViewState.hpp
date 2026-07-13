#pragma once

#include "ViewStateTracker.hpp"
#include "../../helpers/memory/Memory.hpp"

namespace Desktop {
    class CViewState : public IViewStateTracker {
      public:
        CViewState()                   = default;
        virtual ~CViewState() override = default;

        virtual const std::vector<PHLWINDOW>&  windows() const override;
        virtual const std::vector<PHLLS>&      layers() const override;
        virtual const std::vector<PHLVIEWREF>& otherViews() const override;
    };

    UP<CViewState>& viewState();
}
