#pragma once

#include "../../workspace/AbstractWorkspace.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace State::Workspace {
    struct SWorkspaceIdentity {
        ::Workspace::WorkspaceID    id;
        std::string                 address;
        ::Workspace::eWorkspaceType type = ::Workspace::eWorkspaceType::NORMAL;

        bool                        operator==(const SWorkspaceIdentity&) const = default;
    };

    struct SMonitorSnapshot {
        std::string                       address;
        std::optional<SWorkspaceIdentity> activeWorkspace;
        bool                              fallback = false;
    };

    struct SWorkspaceSnapshot {
        SWorkspaceIdentity         identity;
        std::optional<std::string> monitorAddress;
    };

    struct SDefaultWorkspaceCandidate {
        SWorkspaceIdentity         identity;
        std::optional<std::string> boundMonitorAddress;
    };

    class IPolicyContext {
      public:
        virtual ~IPolicyContext() = default;

        // Mutation callbacks are synchronous: later snapshot reads must observe their effects.
        virtual std::vector<SMonitorSnapshot>           monitors() const                                                                       = 0;
        virtual std::vector<SWorkspaceSnapshot>         workspaces() const                                                                     = 0;
        virtual std::optional<SWorkspaceIdentity>       configuredDefaultWorkspace(std::string_view monitorAddress) const                      = 0;
        virtual std::vector<SDefaultWorkspaceCandidate> defaultWorkspaceCandidates(std::string_view monitorAddress) const                      = 0;
        virtual void                                    createWorkspace(const SWorkspaceIdentity& identity, std::string_view monitorAddress)   = 0;
        virtual void                                    moveWorkspace(const SWorkspaceIdentity& identity, std::string_view monitorAddress)     = 0;
        virtual void                                    activateWorkspace(const SWorkspaceIdentity& identity, std::string_view monitorAddress) = 0;
    };

    class CMonitorLifecyclePolicy {
      public:
        // The event monitor must be present in context.monitors() for both calls.
        void                                     monitorConnected(const SMonitorSnapshot& monitor, IPolicyContext& context);
        void                                     monitorDisconnected(const SMonitorSnapshot& monitor, IPolicyContext& context);

        void                                     workspaceDestroyed(const SWorkspaceIdentity& identity);
        void                                     workspaceIdentityChanged(const SWorkspaceIdentity& oldIdentity, const SWorkspaceIdentity& newIdentity);
        void                                     clear();

        std::optional<SWorkspaceIdentity>        rememberedActiveWorkspace(std::string_view monitorAddress) const;
        std::optional<std::string>               returnMonitorAddress(const SWorkspaceIdentity& identity) const;

        static std::optional<SWorkspaceIdentity> selectDefaultWorkspace(std::string_view monitorAddress, const std::optional<SWorkspaceIdentity>& configuredDefault,
                                                                        std::span<const SDefaultWorkspaceCandidate> candidates, std::span<const SWorkspaceSnapshot> workspaces);

      private:
        std::unordered_map<std::string, SWorkspaceIdentity>     m_rememberedActiveWorkspaces;
        std::vector<std::pair<SWorkspaceIdentity, std::string>> m_returnMonitors;
    };
}
