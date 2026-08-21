#pragma once

#include "../Overview.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"

#include <utility>
#include <vector>

namespace Monitor {
    class CMonitorResources;
}
namespace Layout {
    class ITarget;
}
namespace Pointer {
    class CPointerTransformer;
}
class IKeyboard;

namespace Overview::Hyprland {
    class COverviewScene;

    class COverview : public Overview::IOverview {
      public:
        COverview();
        virtual ~COverview() override;

        virtual void         open(PHLMONITOR monitor) override;
        virtual void         close() override;
        virtual bool         isOpen() const override;
        virtual bool         shouldRenderWorkspace(PHLWORKSPACE workspace) const override;
        virtual PHLWORKSPACE inputWorkspace() const override;

      private:
        void                             finishClose(bool emitEvent = true);
        void                             closeImmediately();
        void                             installListeners();
        void                             recheckDrag();
        void                             applyDragHoverTarget();
        void                             releaseDragFromOverview();
        void                             resetDragHover();
        bool                             handleSearchKey(uint32_t keycode, SP<IKeyboard> keyboard, bool repeat = false);
        void                             startKeyRepeat(uint32_t keycode, SP<IKeyboard> keyboard);
        void                             stopKeyRepeat(uint32_t keycode);

        bool                             m_isOpen         = false;
        bool                             m_sceneInstalled = false;
        PHLMONITORREF                    m_monitor;
        WP<Monitor::CMonitorResources>   m_resources;
        PHLANIMVAR<float>                m_progress;
        SP<COverviewScene>               m_scene;
        SP<Pointer::CPointerTransformer> m_pointerTransformer;

        struct {
            CHyprSignalListener monitorDisconnect;
            CHyprSignalListener monitorModeChanged;
            CHyprSignalListener monitorPreRender;
            CHyprSignalListener mouseButton;
            CHyprSignalListener mouseMove;
            CHyprSignalListener sessionLock;
            CHyprSignalListener keyboardKey;
            CHyprSignalListener dragMotion;
            CHyprSignalListener dragEnded;
        } m_listeners;

        enum class eDragHoverTarget : uint8_t {
            NONE,
            LEFT_EDGE,
            RIGHT_EDGE,
            MINI_TILE,
        };

        struct {
            eDragHoverTarget    target = eDragHoverTarget::NONE;
            PHLWORKSPACEREF     workspace;
            WP<Layout::ITarget> dragTarget;
            SP<CEventLoopTimer> eventLoopTimer;
        } m_drag;

        struct {
            WP<IKeyboard>       keyboard;
            uint32_t            keycode = 0;
            SP<CEventLoopTimer> timer;
        } m_keyRepeat;

        enum class eInputMode : uint8_t {
            NAVIGATION,
            TEXT
        } m_inputMode = COverview::eInputMode::NAVIGATION;

        std::vector<std::pair<WP<IKeyboard>, uint32_t>> m_interceptedKeys;
        std::vector<uint32_t>                           m_interceptedButtons;

        friend class COverviewScene;
    };
};
