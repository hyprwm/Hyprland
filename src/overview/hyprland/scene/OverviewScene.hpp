#pragma once

#include "../../../render/scene/Scene.hpp"
#include "../../../desktop/DesktopTypes.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../helpers/math/Math.hpp"

#include <string>

namespace Monitor {
    class CMonitorResources;
}

namespace Overview::Hyprland {
    class COverview;
    class CWorkspaceSearchController;
    class CWorkspaceTapeController;

    bool matchesName(std::string_view a, std::string_view b);

    class COverviewScene : public Render::IScene {
      public:
        COverviewScene(COverview& parent);
        virtual ~COverviewScene() override;

        virtual void draw(Render::CRenderingContext&, Time::steady_tp tp) override;
        void         start(PHLMONITOR monitor, WP<Monitor::CMonitorResources> resources);
        bool         navigateLeft();
        bool         navigateRight();
        PHLWORKSPACE selectedWorkspace() const;
        bool         pointerMove(const Vector2D& monitorLocal);
        bool         pointerButton(uint32_t button, bool pressed, const Vector2D& monitorLocal);
        void         pointerLeave();
        void         keyboardKey(uint32_t keysym, bool down, bool repeat, std::string utf8, uint32_t modifiers);
        void         reset();
        void         setTextboxFocus(bool x);

      private:
        COverview&                     m_parent;
        UP<CWorkspaceTapeController>   m_workspaceTape;
        UP<CWorkspaceSearchController> m_workspaceSearch;
    };
}
