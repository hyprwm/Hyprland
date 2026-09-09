#pragma once

#include "AbstractWorkspace.hpp"
#include "WorkspaceWindowFocusTracker.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "../helpers/signal/Signal.hpp"

#include <optional>
#include <string>

namespace Layout {
    class CSpace;
}

namespace Config {
    class CWorkspaceRule;
}

namespace Workspace {
    class CHLWorkspace : public IAbstractWorkspace {
      public:
        ~CHLWorkspace() override;

        WorkspaceID                         id() const override;
        const std::string&                  displayName() const override;
        const std::string&                  addressableName() const override;
        SP<Monitor::IMonitorAddressable>    monitor() const override;

        SP<Layout::CSpace>                  space() const;
        std::optional<WorkspaceIDContainer> numberedID() const;
        bool                                visible() const;
        void                                setVisible(bool visible);

        WP<CHLWorkspace>                    m_self;

        PHLMONITORREF                       m_monitor;

        PHLANIMVAR<Vector2D>                m_renderOffset;
        PHLANIMVAR<float>                   m_alpha;
        bool                                m_forceRendering = false;
        std::optional<std::string>          m_animationStyle;

        bool                                m_wasCreatedEmpty = true;

        MONITORID                           monitorID() const;
        PHLWINDOW                           getLastFocusedWindow() const;
        void                                rememberFocusedWindow(PHLWINDOW window);
        PHLWINDOW                           getFocusCandidate() const;
        bool                                matchesStaticSelector(const std::string& selector) const;
        void                                updateWindowDecos();
        void                                updateWindowData();
        int                                 getWindowCount(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyPinned = {}, std::optional<bool> onlyVisible = {}) const;
        int                                 getGroups(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyPinned = {}, std::optional<bool> onlyVisible = {}) const;
        bool                                hasUrgentWindow() const;
        PHLWINDOW                           getFirstWindow() const;
        PHLWINDOW                           getTopLeftWindow() const;
        bool                                isVisibleNotCovered() const;
        void                                rename(const std::string& name = "");
        void                                changeID(SWorkspaceNumberedID id);
        void                                forceReportSizesToWindows();
        void                                updateWindows();

        struct {
            CSignalT<> destroy;
            CSignalT<> renamed;
            CSignalT<> idChanged;
            CSignalT<> monitorChanged;
            CSignalT<> activeChanged;
        } m_events;

      protected:
        CHLWorkspace(WorkspaceID id, PHLMONITOR monitor, std::string displayName, std::string addressableName, eWorkspaceType type, bool isEmpty = true);

        void         init(PHLWORKSPACE self);
        virtual void applyTypeSpecificRules(const Config::CWorkspaceRule&);

      private:
        SP<Layout::CSpace>           m_space;
        CWorkspaceWindowFocusTracker m_focusTracker;
        std::string                  m_name       = "";
        bool                         m_visible    = false;
        bool                         m_wasRenamed = false;
        WorkspaceID                  m_id;
        std::string                  m_addressableName;
    };
    inline bool valid(const PHLWORKSPACE& ref) {
        return !!ref;
    }
}
