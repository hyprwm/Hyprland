#pragma once

#include "../../helpers/time/Time.hpp"

namespace Render {
    class CRenderContext;
    class IScene {
      public:
        virtual ~IScene() = default;

        virtual void draw(CRenderContext& ctx, const Time::steady_tp& now) = 0;

      protected:
        IScene() = default;
    };
}
