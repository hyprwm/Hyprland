#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

#include "../../../helpers/math/Math.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../helpers/signal/Signal.hpp"
#include "../../../defines.hpp"
#include "../../DesktopTypes.hpp"

class CWLSurfaceResource;

namespace Desktop::View {
    class CWindow;
    class IPopupBackend;

    enum class eBackendType : uint8_t {
        WINDOW_BACKEND_WAYLAND = 0,
        WINDOW_BACKEND_X11,
    };

    enum class eBackendState : uint8_t {
        BACKEND_STATE_CURRENT = 0,
        BACKEND_STATE_PENDING,
    };

    enum class eBackendResizeEdge : uint8_t {
        BACKEND_RESIZE_EDGE_NONE = 0,
        BACKEND_RESIZE_EDGE_TOP,
        BACKEND_RESIZE_EDGE_BOTTOM,
        BACKEND_RESIZE_EDGE_LEFT,
        BACKEND_RESIZE_EDGE_RIGHT,
        BACKEND_RESIZE_EDGE_TOP_LEFT,
        BACKEND_RESIZE_EDGE_TOP_RIGHT,
        BACKEND_RESIZE_EDGE_BOTTOM_LEFT,
        BACKEND_RESIZE_EDGE_BOTTOM_RIGHT,
    };

    struct SClientGeometry {
        CBox box;
        bool positionAuthoritative = false;
    };

    struct SGeometryHints {
        std::optional<Vector2D> minSize;
        std::optional<Vector2D> maxSize;
    };

    struct SBackendMetadata {
        std::string                title;
        std::string                appID;
        std::optional<std::string> tag;
        std::optional<std::string> description;
    };

    struct SBackendTraits {
        bool overrideRedirect : 1       = false;
        bool modal : 1                  = false;
        bool hasModalChild : 1          = false;
        bool transient : 1              = false;
        bool wantsFocus : 1             = true;
        bool suggestsFloat : 1          = false;
        bool suggestsNoInitialFocus : 1 = false;
        bool preventsFocus : 1          = false;
        bool suggestsNoBorder : 1       = false;
        bool fullscreen : 1             = false;
    };

    struct SBackendStateRequest {
        std::optional<bool>      fullscreen;
        std::optional<MONITORID> fullscreenMonitor;
        std::optional<bool>      maximized;
        std::optional<bool>      minimized;
    };

    struct SBackendClientID {
        eBackendType type = eBackendType::WINDOW_BACKEND_WAYLAND;
        uint64_t     id   = 0;

        bool         operator==(const SBackendClientID&) const = default;
    };

    class CWindowConfigureAckTracker {
      public:
        void                    add(uint32_t serial, const Vector2D& size);
        std::optional<Vector2D> acknowledge(uint32_t serial);
        bool                    empty() const;

      private:
        std::vector<std::pair<uint32_t, Vector2D>> m_pending;
    };

    class IWindowBackend {
      public:
        virtual ~IWindowBackend();

        virtual bool                   valid() const    = 0;
        virtual bool                   isMapped() const = 0;
        virtual eBackendType           type() const     = 0;
        bool                           isX11() const;
        virtual pid_t                  pid() const      = 0;
        virtual SP<CWLSurfaceResource> surface() const  = 0;
        virtual PHLWINDOW              parent() const   = 0;
        virtual SBackendClientID       clientID() const = 0;

        virtual bool                   initialCommit() const                    = 0;
        virtual SClientGeometry        geometry() const                         = 0;
        virtual SGeometryHints         geometryHints(eBackendState state) const = 0;
        virtual SBackendMetadata       metadata() const                         = 0;
        virtual SBackendTraits         traits() const                           = 0;
        virtual double                 surfaceScale() const                     = 0;
        virtual Vector2D               reportedSize() const                     = 0;

        virtual CBox                   clientToLogical(const CBox& box, PHLMONITOR preferredMonitor) const = 0;
        virtual CBox                   logicalToClient(const CBox& box, PHLMONITOR preferredMonitor) const = 0;
        virtual Vector2D               surfaceLocalToBuffer(const Vector2D& local) const                   = 0;
        virtual Vector2D               bufferToSurfaceLocal(const Vector2D& buffer) const                  = 0;

        virtual void                   configure(const CBox& logicalBox, PHLMONITOR preferredMonitor, bool force = false) = 0;
        virtual void                   acknowledgeConfigure(const CBox& clientBox)                                        = 0;
        virtual void                   setActive(bool active)                                                             = 0;
        virtual void                   setFullscreen(bool fullscreen)                                                     = 0;
        virtual void                   setMaximized(bool maximized)                                                       = 0;
        virtual void                   setResizing(bool resizing)                                                         = 0;
        virtual bool                   setSuspended(bool suspended)                                                       = 0;
        virtual void                   setMinimized(bool minimized)                                                       = 0;
        virtual void                   restackToTop()                                                                     = 0;
        virtual void                   close()                                                                            = 0;
        virtual void                   ping()                                                                             = 0;

        struct {
            CSignalT<>                       map;
            CSignalT<>                       unmap;
            CSignalT<bool>                   commit;
            CSignalT<>                       destroy;
            CSignalT<SP<CWLSurfaceResource>> surfaceChanged;
            CSignalT<SBackendMetadata>       metadataChanged;
            CSignalT<SBackendTraits>         traitsChanged;
            CSignalT<SBackendStateRequest>   stateRequest;
            CSignalT<CBox>                   configureRequest;
            CSignalT<CBox>                   geometryChanged;
            CSignalT<CBox>                   clientResizeRequest;
            CSignalT<>                       activationRequest;
            CSignalT<>                       moveRequest;
            CSignalT<eBackendResizeEdge>     resizeRequest;
            CSignalT<>                       pong;
            CSignalT<SP<IPopupBackend>>      newPopup;
        } m_events;

        IWindowBackend(const IWindowBackend&)            = delete;
        IWindowBackend(IWindowBackend&&)                 = delete;
        IWindowBackend& operator=(const IWindowBackend&) = delete;
        IWindowBackend& operator=(IWindowBackend&&)      = delete;

      protected:
        IWindowBackend();

        virtual void attach(PHLWINDOWREF window) = 0;

        friend class CWindow;
    };
}
