#pragma once

#include "../../helpers/time/Time.hpp"

namespace Render {
    class IScene {
      public:
        virtual ~IScene() = default;

        virtual void draw(const Time::steady_tp& now) = 0;

      protected:
        IScene() = default;
    };
}
