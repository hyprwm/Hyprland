#pragma once

#include "../../macros/Class.hpp"
#include "../../helpers/signal/Signal.hpp"

namespace Bell {
    class IBellImpl {
      public:
        virtual ~IBellImpl() = default;

        NON_MOVABLE(IBellImpl);

        virtual void play() const = 0;

      protected:
        IBellImpl() = default;
    };
}
