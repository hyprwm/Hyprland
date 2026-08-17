#pragma once

#include "../../../render/scene/Scene.hpp"
#include "../../../helpers/memory/Memory.hpp"

namespace Render {
    class IFramebuffer;
}

namespace Overview::Hyprland {
    class COverview;

    class COverviewScene : public Render::IScene {
      public:
        COverviewScene(COverview& parent);
        virtual ~COverviewScene() override = default;

        virtual void draw(Time::steady_tp tp) override;
        void         reset();

      private:
        COverview&               m_parent;
        SP<Render::IFramebuffer> m_framebuffer;
    };
}
