#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/math/Math.hpp"
#include "../../helpers/signal/Signal.hpp"

#include <cstdint>
#include <string>

namespace Screenshare {
    class CWorkspaceCaptureSource {
      public:
        CWorkspaceCaptureSource(PHLWORKSPACE workspace, uint32_t mode);
        CWorkspaceCaptureSource(const CWorkspaceCaptureSource&) = delete;
        CWorkspaceCaptureSource(CWorkspaceCaptureSource&&)      = delete;

        PHLWORKSPACE       workspace() const;
        PHLMONITOR         monitor() const;
        Vector2D           bufferSize() const;
        const std::string& name() const;
        bool               removed() const;
        uint32_t           mode() const;

        CSignalT<>         m_changed;

      private:
        PHLWORKSPACEREF m_workspace;
        PHLMONITORREF   m_lastMonitor;
        Vector2D        m_bufferSize = {0, 0};
        std::string     m_name;
        bool            m_removed = false;
        uint32_t        m_mode    = 0;

        struct {
            CHyprSignalListener destroy;
            CHyprSignalListener monitorChanged;
            CHyprSignalListener renamed;
            CHyprSignalListener idChanged;
            CHyprSignalListener modeChanged;
        } m_listeners;

        void updateMonitor();
        void updateSize();
    };
}
