#pragma once

#include "Scene.hpp"
#include "../../desktop/DesktopTypes.hpp"

namespace Render {
    class CMonitorScene : public IScene {
      public:
        explicit CMonitorScene(PHLMONITORREF monitor);

        void draw(const Time::steady_tp& now) override;

      private:
        PHLMONITORREF m_monitor;
    };
}
