#pragma once

#include "../Overview.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../managers/SeatManager.hpp"
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
struct SEventLoopDoLaterLock;

namespace Overview::Hyprland {
    class COverviewScene;

    class COverview : public Overview::IOverview, public Overview::IOverviewGestureOpenable, public Overview::IOverviewGestureMovable, public IKeyboardEventHandler {
      public:
        COverview();
        virtual ~COverview() override;

        virtual void         open(PHLMONITOR monitor) override;
        void                 open(PHLMONITOR monitor, const std::string& query);
        virtual void         close() override;
        virtual bool         isOpen() const override;
        SOverviewState       state() const;
        virtual bool         shouldRenderWorkspace(PHLWORKSPACE workspace) const override;
        virtual PHLWORKSPACE inputWorkspace() const override;
        virtual bool         beginOpenGesture(PHLMONITOR monitor) override;
        virtual void         updateOpenGesture(float completion) override;
        virtual void         endOpenGesture(bool commit) override;
        virtual bool         beginMoveGesture() override;
        virtual void         updateMoveGesture(float Δ) override;
        virtual void         endMoveGesture() override;
        bool                 moveLeft();
        bool                 moveRight();
        virtual void         onKeyboardKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard> keyboard) override;

        SP<COverviewScene>   scene() const;

      private:
        void                             finishClose(bool emitEvent = true);
        void                             closeImmediately();
        bool                             prepareOpen(PHLMONITOR monitor, bool& newScene);
        void                             commitClose();
        void                             settleProgress(float goal, bool opening);
        void                             scheduleFinishClose();
        void                             installListeners();
        void                             updatePointerState();
        void                             recheckDrag();
        void                             applyDragHoverTarget();
        void                             releaseDragFromOverview();
        void                             resetDragHover();
        bool                             handleSearchKey(uint32_t keycode, SP<IKeyboard> keyboard, bool repeat = false);
        void                             startKeyRepeat(uint32_t keycode, SP<IKeyboard> keyboard);
        void                             stopKeyRepeat(uint32_t keycode, SP<IKeyboard> keyboard = nullptr);

        bool                             m_isOpen            = false;
        bool                             m_sceneInstalled    = false;
        bool                             m_gestureActive     = false;
        bool                             m_gestureOpening    = false;
        bool                             m_moveGestureActive = false;
        float                            m_gestureStart      = 0.F;
        PHLMONITORREF                    m_monitor;
        WP<Monitor::CMonitorResources>   m_resources;
        PHLANIMVAR<float>                m_progress;
        SP<COverviewScene>               m_scene;
        SP<Pointer::CPointerTransformer> m_pointerTransformer;
        UP<SEventLoopDoLaterLock>        m_finishCloseLock;

        struct {
            CHyprSignalListener monitorDisconnect;
            CHyprSignalListener monitorModeChanged;
            CHyprSignalListener monitorPreRender;
            CHyprSignalListener mouseButton;
            CHyprSignalListener mouseMove;
            CHyprSignalListener sessionLock;
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
            bool                createdWorkspace = false;
        } m_drag;

        struct {
            WP<IKeyboard>       keyboard;
            uint32_t            keycode = 0;
            SP<CEventLoopTimer> timer;
        } m_keyRepeat;

        WP<IKeyboardEventHandler> m_keyboardEventHandler;

        enum class eInputMode : uint8_t {
            NAVIGATION,
            TEXT
        } m_inputMode = COverview::eInputMode::NAVIGATION;

        std::vector<uint32_t> m_interceptedButtons;

        friend class COverviewScene;
    };
};
