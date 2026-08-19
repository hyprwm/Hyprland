#pragma once

#include "../../../desktop/DesktopTypes.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../helpers/math/Math.hpp"
#include "../../../helpers/signal/Signal.hpp"
#include "../../../helpers/time/Time.hpp"

#include <functional>
#include <vector>

namespace Monitor {
    class CMonitorResources;
}
namespace Render {
    class CRenderingContext;
}
namespace Overview::Hyprland::OverviewLayout {
    struct SLayout;
}

namespace Overview::Hyprland {
    class CWorkspaceTapeController {
      public:
        using FWorkspaceFilter = std::function<bool(PHLWORKSPACE)>;

        CWorkspaceTapeController();
        ~CWorkspaceTapeController();

        void         start(PHLMONITOR monitor, WP<Monitor::CMonitorResources> resources, const OverviewLayout::SLayout& layout);
        void         reset();
        void         draw(Render::CRenderingContext&, Time::steady_tp tp, float overviewProgress, size_t reservedWorkBuffers = 0);

        bool         navigateLeft();
        bool         navigateRight();
        bool         selectWorkspace(PHLWORKSPACE workspace);
        PHLWORKSPACE selectedWorkspace() const;
        PHLWORKSPACE miniWorkspaceAt(const Vector2D& monitorLocal) const;
        bool         pointerButton(uint32_t button, bool pressed, const Vector2D& monitorLocal);

        void         setFilter(FWorkspaceFilter filter);
        void         refresh();

      private:
        struct SWorkspaceTile;

        bool                            navigate(int direction);
        void                            reconcile(bool initial = false);
        void                            updateLayout(bool warp = false);
        void                            retireTile(SWorkspaceTile& tile);
        void                            ensureAnimations(SWorkspaceTile& tile);
        void                            installWorkspaceListeners(SWorkspaceTile& tile);
        void                            damageMonitor() const;
        void                            damageMiniStrip() const;
        void                            updateMiniBorderColors(bool warp = false);
        SWorkspaceTile*                 tileFor(PHLWORKSPACE workspace) const;
        SWorkspaceTile*                 tileFor(PHLWORKSPACEREF workspace) const;
        std::vector<PHLWORKSPACE>       filteredWorkspaces() const;
        std::vector<SWorkspaceTile*>    layoutTiles() const;
        void                            pruneRetiredTiles();

        PHLMONITORREF                   m_monitor;
        WP<Monitor::CMonitorResources>  m_resources;
        FWorkspaceFilter                m_filter;
        PHLWORKSPACEREF                 m_selectedWorkspace;
        PHLWORKSPACEREF                 m_preferredWorkspace;
        PHLWORKSPACEREF                 m_pressedMiniWorkspace;
        std::vector<UP<SWorkspaceTile>> m_tiles;
        CBox                            m_mainArea;
        CBox                            m_miniStripArea;
        bool                            m_started = false;

        struct {
            CHyprSignalListener created;
            CHyprSignalListener removed;
            CHyprSignalListener renamed;
            CHyprSignalListener moved;
            CHyprSignalListener active;
            CHyprSignalListener monitorAdded;
            CHyprSignalListener monitorRemoved;
            CHyprSignalListener monitorLayoutChanged;
            CHyprSignalListener monitorPreRender;
            CHyprSignalListener configRefreshed;
        } m_listeners;
    };
}
