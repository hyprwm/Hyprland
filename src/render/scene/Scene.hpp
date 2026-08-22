#pragma once

#include "../../helpers/time/Time.hpp"

namespace Render {
    class CRenderingContext;

    class IScene {
      public:
        virtual ~IScene() = default;

        virtual void draw(CRenderingContext&, Time::steady_tp tp) = 0;

      protected:
        IScene() = default;
    };
}
