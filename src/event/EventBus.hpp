#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/math/Math.hpp"

#include "../devices/IPointer.hpp"
#include "../devices/IKeyboard.hpp"
#include "../devices/Tablet.hpp"
#include "../devices/ITouch.hpp"

#include "../desktop/DesktopTypes.hpp"

#include "../SharedDefs.hpp"
#include <unordered_map>
#include <variant>

namespace Desktop {
    enum eFocusReason : uint8_t;
}
namespace Event {
    struct SCallbackInfo {
        bool cancelled = false; /* on cancellable events, will cancel the event. */
    };

    class CEventBus {
      public:
        CEventBus()  = default;
        ~CEventBus() = default;

        template <typename... Args>
        using Event = CSignalT<Args...>;

        template <typename... Args>
        using Cancellable = CSignalT<Args..., SCallbackInfo&>;

        class CCustomEvent {
          public:
            // maps a name to a ValidVariant
            // must have the same order as ValidVariant
            enum eType : uint8_t {
                TYPE_BOOL          = 0,
                TYPE_INT           = 1,
                TYPE_DOUBLE        = 2,
                TYPE_STRING        = 3,
                TYPE_WINDOW        = 4,
                TYPE_WORKSPACE     = 5,
                TYPE_LAYER_SURFACE = 6,
                TYPE_MONITOR       = 7,
            };

            using ValidVariant = std::variant<bool, int, double, std::string, PHLWINDOW, PHLWORKSPACE, PHLLS, PHLMONITOR>;

            CCustomEvent(std::string name, std::vector<eType> argTypes);
            ~CCustomEvent();

            std::expected<void, std::string>        emit(const std::vector<ValidVariant>& args);

            std::string                             m_name;
            std::vector<eType>                      m_argTypes;
            Event<const std::vector<ValidVariant>&> m_event;
        };

        struct {
            Event<> ready;
            Event<> tick;
            Event<> start;
            Event<> exit;

            struct {
                Event<PHLWINDOW>                        open;
                Event<PHLWINDOW>                        openEarly;
                Event<PHLWINDOW>                        destroy;
                Event<PHLWINDOW>                        close;
                Event<PHLWINDOW>                        kill;
                Event<PHLWINDOW, Desktop::eFocusReason> active;
                Event<PHLWINDOW>                        urgent;
                Event<PHLWINDOW>                        title;
                Event<PHLWINDOW>                        class_;
                Event<PHLWINDOW>                        pin;
                Event<PHLWINDOW>                        fullscreen;
                Event<PHLWINDOW>                        updateRules;
                Event<PHLWINDOW, PHLWORKSPACE>          moveToWorkspace;
            } window;

            struct {
                Event<PHLLS> opened;
                Event<PHLLS> closed;
                Event<PHLLS> updateRules;
            } layer;

            struct {
                struct {
                    Cancellable<Vector2D>               move;
                    Cancellable<IPointer::SButtonEvent> button;
                    Cancellable<IPointer::SAxisEvent>   axis;
                } mouse;

                struct {
                    Cancellable<IKeyboard::SKeyEvent>        key;
                    Event<SP<IKeyboard>, const std::string&> layout;
                    Event<SP<CWLSurfaceResource>>            focus;
                } keyboard;

                struct {
                    Cancellable<CTablet::SAxisEvent>      axis;
                    Cancellable<CTablet::SButtonEvent>    button;
                    Cancellable<CTablet::SProximityEvent> proximity;
                    Cancellable<CTablet::STipEvent>       tip;
                } tablet;

                struct {
                    Cancellable<ITouch::SCancelEvent> cancel;
                    Cancellable<ITouch::SDownEvent>   down;
                    Cancellable<ITouch::SUpEvent>     up;
                    Cancellable<ITouch::SMotionEvent> motion;
                } touch;
            } input;

            struct {
                Event<PHLMONITOR>   preChecks;
                Event<PHLMONITOR>   pre;
                Event<eRenderStage> stage;
            } render;

            struct {
                Event<bool /* state start/stop */, uint8_t /* eScreenshareType */, const std::string& /* name */> state;
            } screenshare;

            struct {
                struct {
                    Cancellable<IPointer::SSwipeBeginEvent>  begin;
                    Cancellable<IPointer::SSwipeEndEvent>    end;
                    Cancellable<IPointer::SSwipeUpdateEvent> update;
                } swipe;

                struct {
                    Cancellable<IPointer::SPinchBeginEvent>  begin;
                    Cancellable<IPointer::SPinchEndEvent>    end;
                    Cancellable<IPointer::SPinchUpdateEvent> update;
                } pinch;
            } gesture;

            struct {
                Event<PHLMONITOR> newMon;
                Event<PHLMONITOR> preAdded;
                Event<PHLMONITOR> added;
                Event<PHLMONITOR> preRemoved;
                Event<PHLMONITOR> removed;
                Event<PHLMONITOR> preCommit;
                Event<PHLMONITOR> focused;

                Event<>           layoutChanged;
            } monitor;

            struct {
                Event<PHLWORKSPACE, PHLMONITOR> moveToMonitor;
                Event<PHLWORKSPACE>             active;
                Event<PHLWORKSPACE, PHLMONITOR> specialActive;
                Event<PHLWORKSPACEREF>          created;
                Event<PHLWORKSPACEREF>          removed;
            } workspace;

            struct {
                Event<> preReload;
                Event<> reloaded;
            } config;

            struct {
                Event<const std::string&> submap;
            } keybinds;

            Event<SP<CCustomEvent>>                           pluginEventAdded;
            Event<std::string>                                pluginEventRemoved;
            std::unordered_map<std::string, SP<CCustomEvent>> plugin;

        } m_events;
    };

    UP<CEventBus>& bus();
};
