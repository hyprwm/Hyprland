#pragma once

#include "../Overview.hpp"
#include "../../helpers/AnimatedVariable.hpp"

namespace Monitor {
    class CMonitorResources;
}

namespace Overview::Hyprland {
    class COverviewScene;

    class COverview : public Overview::IOverview {
      public:
        COverview();
        virtual ~COverview() override;

        virtual void open(PHLMONITOR monitor) override;
        virtual void close() override;
        virtual bool isOpen() const override;

      private:
        void                           finishClose(bool emitEvent = true);
        void                           closeImmediately();

        bool                           m_isOpen         = false;
        bool                           m_sceneInstalled = false;
        PHLMONITORREF                  m_monitor;
        WP<Monitor::CMonitorResources> m_resources;
        PHLANIMVAR<float>              m_progress;
        SP<COverviewScene>             m_scene;

        struct {
            CHyprSignalListener monitorDisconnect;
            CHyprSignalListener monitorModeChanged;
            CHyprSignalListener monitorPreRender;
            CHyprSignalListener sessionLock;
        } m_listeners;

        friend class COverviewScene;
    };
};
