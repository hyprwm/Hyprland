#pragma once

#include <optional>

#include "WindowBackend.hpp"
#include "../types/ClientMappable.hpp"

class CXWaylandSurface;

namespace Desktop::View {
    class CX11Backend final : public IWindowBackend, public IClientMappable {
      public:
        CX11Backend(SP<CXWaylandSurface> surface);

        bool                   valid() const override;
        bool                   isMapped() const override;
        eBackendType           type() const override;
        pid_t                  pid() const override;
        SP<CWLSurfaceResource> surface() const override;
        PHLWINDOW              parent() const override;
        SBackendClientID       clientID() const override;

        bool                   initialCommit() const override;
        SClientGeometry        geometry() const override;
        SGeometryHints         geometryHints(eBackendState state) const override;
        SBackendMetadata       metadata() const override;
        SBackendTraits         traits() const override;
        double                 surfaceScale() const override;
        Vector2D               reportedSize() const override;

        CBox                   clientToLogical(const CBox& box, PHLMONITOR preferredMonitor) const override;
        CBox                   logicalToClient(const CBox& box, PHLMONITOR preferredMonitor) const override;
        Vector2D               surfaceLocalToBuffer(const Vector2D& local) const override;
        Vector2D               bufferToSurfaceLocal(const Vector2D& buffer) const override;

        void                   configure(const CBox& logicalBox, PHLMONITOR preferredMonitor, bool force = false) override;
        void                   acknowledgeConfigure(const CBox& clientBox) override;
        void                   setActive(bool active) override;
        void                   setFullscreen(bool fullscreen) override;
        void                   setMaximized(bool maximized) override;
        void                   setResizing(bool resizing) override;
        bool                   setSuspended(bool suspended) override;
        void                   setMinimized(bool minimized) override;
        void                   restackToTop() override;
        void                   close() override;
        void                   ping() override;

      private:
        void                    attach(PHLWINDOWREF window) override;
        PHLMONITOR              preferredMonitor(PHLMONITOR monitor) const;
        void                    updateGeometry(bool emitEvent);
        void                    updateMetadata(bool emitEvent);
        void                    updateTraits(bool emitEvent);
        void                    updateSurface(bool emitEvent);

        WP<CXWaylandSurface>    m_xwaylandSurface;
        WP<CWLSurfaceResource>  m_surface;
        PHLWINDOWREF            m_window;

        pid_t                   m_pid       = -1;
        SBackendClientID        m_clientID  = {.type = eBackendType::WINDOW_BACKEND_X11};
        bool                    m_destroyed = false;
        bool                    m_mapped    = false;

        SClientGeometry         m_geometry;
        SGeometryHints          m_geometryHints;
        SBackendMetadata        m_metadata;
        SBackendTraits          m_traits;
        Vector2D                m_reportedPosition;
        Vector2D                m_reportedSize;
        std::optional<Vector2D> m_pendingReportedSize;

        struct {
            CHyprSignalListener map;
            CHyprSignalListener unmap;
            CHyprSignalListener commit;
            CHyprSignalListener destroy;
            CHyprSignalListener resourceChange;
            CHyprSignalListener state;
            CHyprSignalListener metadata;
            CHyprSignalListener configureRequest;
            CHyprSignalListener setGeometry;
            CHyprSignalListener activate;
            CHyprSignalListener pong;
        } m_listeners;
    };
}
