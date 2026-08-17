#pragma once

#include "Scene.hpp"
#include "../../desktop/DesktopTypes.hpp"

namespace Render {
    class CMonitorScene : public IScene {
      public:
        CMonitorScene(PHLMONITOR mon);
        ~CMonitorScene() = default;

        virtual void draw(Time::steady_tp tp) override;

      private:
        PHLMONITORREF m_monitor;
    };
};
