#pragma once

#include "../../helpers/time/Time.hpp"

namespace Render {
    class IScene {
      public:
        virtual ~IScene() = default;

        virtual void draw(Time::steady_tp tp) = 0;

      protected:
        IScene() = default;
    };
}
