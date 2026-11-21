#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"
#include <hyprutils/os/FileDescriptor.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-server-core.h>

class CSeatManager;
class IDataSource;
class IPointer;

namespace Desktop {
    class CFocusState;
}

namespace IPC::Socket1 {
    class CUnixImpl;
}

namespace IPC::Socket2 {
    class CUnixImpl;
}

/*
    An actor is someone working on this machine: the human or an agent. Each actor has its own seat, focus, cursor
    and clipboard, so actors work side by side without taking each other's keyboard, focus, cursor or clipboard.

    The human owns the real devices and the main socket. An agent owns a socket and a headless monitor named after
    it: its clients connect to that socket, their windows open on that monitor, and its virtual pointer moves a
    cursor that stays on it. A client sees only its actor's monitors and windows, and an agent never sees what acts
    on the whole machine, such as the lock screen or the monitor layout.

    Code runs as one actor at a time, the current actor. A client request runs as the client's actor, so an agent's
    virtual keyboard types as the agent. A monitor renders as its actor. Real devices, timers, hyprctl and X11 run as
    the human, and deferred work runs as the actor that deferred it. g_pSeatManager and Desktop::focusState() belong
    to the current actor.

    Each actor has its own hyprctl and event sockets: the human the instance's, and an agent those in a folder named
    after it in the instance's, so HYPRLAND_INSTANCE_SIGNATURE=<instance>/<agent> reaches it. A request on them runs
    as their actor and lists only its monitors, workspaces and windows, and an event reaches only the actor it
    happened to. Apps an actor starts get its socket and signature, and an agent's get no X11, which is the human's.

    Agents come from HYPRLAND_ACTORS, a comma separated list of name=socket.
*/
namespace Actor {
    class CActor {
      public:
        CActor(std::string name, std::string socket);
        ~CActor();

        bool isHuman() const;
        bool owns(wl_client* client) const;
        bool owns(PHLWINDOW window) const;
        bool owns(PHLMONITOR monitor) const;
        bool owns(PHLWORKSPACE workspace) const;

        // the Wayland socket its apps connect to, and the HYPRLAND_INSTANCE_SIGNATURE that reaches its hyprctl
        std::string display() const;
        std::string signature() const;
        Vector2D    cursor() const;

        // moves this actor's cursor and clicks with it, instead of the human's
        void attachPointer(SP<IPointer> pointer);

        // the actor a socket's callback serves: the one in its data, or the human for none
        static SP<CActor>        from(void* data);

        const std::string        m_name;
        const std::string        m_socket;
        UP<CSeatManager>         m_seat;
        SP<Desktop::CFocusState> m_focus;

        // its own hyprctl and event sockets; the human's are the instance's
        UP<IPC::Socket1::CUnixImpl> m_hyprctl;
        UP<IPC::Socket2::CUnixImpl> m_events;

      private:
        struct SPointer {
            WP<IPointer>        pointer;
            CHyprSignalListener motion;
            CHyprSignalListener motionAbsolute;
            CHyprSignalListener button;
            CHyprSignalListener axis;
            CHyprSignalListener frame;
            CHyprSignalListener destroy;
        };

        struct SSocket {
            Hyprutils::OS::CFileDescriptor fd;
            wl_event_source*               source = nullptr;
            std::string                    path;
        };

        bool                           listen();
        bool                           listenOn(const std::string& path, wl_event_loop_fd_func_t onReadable);
        static int                     onListen(int fd, uint32_t mask, void* data);
        void                           movePointer(const Vector2D& pos, uint32_t timeMs);
        PHLWINDOW                      windowAt(const Vector2D& pos) const;

        WP<CActor>                     m_self;
        Hyprutils::OS::CFileDescriptor m_lockFD;
        std::string                    m_socketPath;
        std::string                    m_ipcPath;
        std::vector<SSocket>           m_sockets;
        Vector2D                       m_cursor;
        std::vector<UP<SPointer>>      m_pointers;

        friend class CActorManager;
    };

    class CActorManager {
      public:
        CActorManager();
        ~CActorManager();

        SP<CActor> human() const;
        SP<CActor> current() const;
        SP<CActor> actorFor(wl_client* client) const;
        SP<CActor> actorFor(PHLMONITOR monitor) const;
        SP<CActor> actorFor(PHLWINDOW window) const;

        // the workspace an agent's monitor starts on, named after the agent so it leaves the human's numbers alone
        std::optional<std::string> workspaceFor(PHLMONITOR monitor) const;

        // whether the source is the selection or primary selection of an actor other than the current one
        bool isOthersSelection(const SP<IDataSource>& source) const;

        // whether the client may not see it: another actor's window or monitor, or, for an agent, a global that
        // acts on the whole machine
        bool hides(const wl_client* client, PHLWINDOW window) const;
        bool hides(const wl_client* client, const wl_global* global) const;

      private:
        struct SClient {
            wl_listener destroy     = {};
            wl_listener destroyLate = {};
            WP<CActor>  actor;
            WP<CActor>  previous;
        };

        void                                        createMonitors();

        static void                                 onClientCreated(wl_listener* listener, void* data);
        static void                                 onClientDestroy(wl_listener* listener, void* data);
        static void                                 onClientDestroyLate(wl_listener* listener, void* data);
        static void                                 onRequest(void* data, wl_protocol_logger_type type, const wl_protocol_logger_message* message);

        std::vector<SP<CActor>>                     m_actors;
        WP<CActor>                                  m_current;
        std::unordered_map<wl_client*, UP<SClient>> m_clients;
        wl_listener                                 m_clientCreated = {};
        wl_protocol_logger*                         m_logger        = nullptr;
        CHyprSignalListener                         m_monitorAdded;

        friend class CActor;
        friend class CScope;
    };

    // Runs the rest of a block as an actor, then returns to the previous actor
    class CScope {
      public:
        explicit CScope(SP<CActor> actor);
        ~CScope();

        CScope(const CScope&)            = delete;
        CScope& operator=(const CScope&) = delete;

      private:
        WP<CActor> m_previous;
    };

    UP<CActorManager>& mgr();
}
