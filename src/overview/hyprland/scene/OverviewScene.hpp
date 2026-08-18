#pragma once

#include "../../../render/scene/Scene.hpp"
#include "../../../desktop/DesktopTypes.hpp"
#include "../../../helpers/memory/Memory.hpp"

namespace Monitor {
    class CMonitorResources;
}

namespace Overview::Hyprland {
    class COverview;
    class CWorkspaceTapeController;

    class COverviewScene : public Render::IScene {
      public:
        COverviewScene(COverview& parent);
        virtual ~COverviewScene() override;

        virtual void                        draw(Time::steady_tp tp) override;
        void                                start(PHLMONITOR monitor, WP<Monitor::CMonitorResources> resources);
        bool                                navigateLeft();
        bool                                navigateRight();
        PHLWORKSPACE                        selectedWorkspace() const;
        void                                reset();

      private:
        COverview&                   m_parent;
        UP<CWorkspaceTapeController> m_workspaceTape;
    };
}
