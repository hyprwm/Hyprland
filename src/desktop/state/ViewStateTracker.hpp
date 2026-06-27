#pragma once

#include "../DesktopTypes.hpp"
#include "ViewHitTester.hpp"
#include "ViewQuery.hpp"

#include <vector>

namespace Desktop {
    class IViewStateTracker {
      public:
        virtual ~IViewStateTracker() = default;

        virtual const std::vector<PHLWINDOW>&  windows() const    = 0;
        virtual const std::vector<PHLLS>&      layers() const     = 0;
        virtual const std::vector<PHLVIEWREF>& otherViews() const = 0;

        virtual CViewQuery                     query() const;
        virtual CViewHitTester                 hitTest() const;

      protected:
        IViewStateTracker() = default;
    };
}
