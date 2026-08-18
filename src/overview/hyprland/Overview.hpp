#pragma once

#include "../Overview.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../helpers/time/Timer.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"

#include <utility>
#include <vector>

namespace Monitor {
    class CMonitorResources;
}
class IKeyboard;

namespace Overview::Hyprland {
    class COverviewScene;

    class COverview : public Overview::IOverview {
      public:
        COverview();
        virtual ~COverview() override;

        virtual void open(PHLMONITOR monitor) override;
        virtual void close() override;
        virtual bool isOpen() const override;
        virtual bool shouldRenderWorkspace(PHLWORKSPACE workspace) const override;

      private:
        void                           finishClose(bool emitEvent = true);
        void                           closeImmediately();
        void                           installListeners();
        void                           recheckDrag();

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
            CHyprSignalListener mouseButton;
            CHyprSignalListener mouseMove;
            CHyprSignalListener sessionLock;
            CHyprSignalListener keyboardKey;
        } m_listeners;

        struct {
            CTimer debouncer;
            // met criteria
            bool                isWithin = false;
            SP<CEventLoopTimer> eventLoopTimer;
        } m_drag;

        std::vector<std::pair<WP<IKeyboard>, uint32_t>> m_interceptedKeys;

        friend class COverviewScene;
    };
};
