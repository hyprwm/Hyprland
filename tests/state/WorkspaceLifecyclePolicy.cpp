#include <state/workspace/LifecyclePolicy.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace State::Workspace;

struct SRecordedAction {
    enum class eType {
        CREATE,
        MOVE,
        ACTIVATE,
    };

    eType       type;
    std::string workspace;
    std::string monitor;

    bool        operator==(const SRecordedAction&) const = default;
};

class CFakePolicyContext final : public IPolicyContext {
  public:
    std::vector<SMonitorSnapshot> monitors() const override {
        return m_monitors;
    }

    std::vector<SWorkspaceSnapshot> workspaces() const override {
        return m_workspaces;
    }

    std::optional<SWorkspaceIdentity> configuredDefaultWorkspace(std::string_view monitorAddress) const override {
        const auto DEFAULT = m_configuredDefaults.find(std::string{monitorAddress});
        return DEFAULT == m_configuredDefaults.end() ? std::nullopt : std::optional{DEFAULT->second};
    }

    std::vector<SDefaultWorkspaceCandidate> defaultWorkspaceCandidates(std::string_view monitorAddress) const override {
        const auto CANDIDATES = m_defaultCandidates.find(std::string{monitorAddress});
        return CANDIDATES == m_defaultCandidates.end() ? std::vector<SDefaultWorkspaceCandidate>{} : CANDIDATES->second;
    }

    void createWorkspace(const SWorkspaceIdentity& identity, std::string_view monitorAddress) override {
        m_actions.emplace_back(SRecordedAction{SRecordedAction::eType::CREATE, identity.address, std::string{monitorAddress}});
        m_workspaces.emplace_back(SWorkspaceSnapshot{identity, std::string{monitorAddress}});
    }

    void moveWorkspace(const SWorkspaceIdentity& identity, std::string_view monitorAddress) override {
        m_actions.emplace_back(SRecordedAction{SRecordedAction::eType::MOVE, identity.address, std::string{monitorAddress}});

        const auto WORKSPACE = std::ranges::find(m_workspaces, identity, &SWorkspaceSnapshot::identity);
        ASSERT_NE(WORKSPACE, m_workspaces.end());
        WORKSPACE->monitorAddress = monitorAddress;

        if (m_onMove)
            m_onMove();
    }

    void activateWorkspace(const SWorkspaceIdentity& identity, std::string_view monitorAddress) override {
        m_actions.emplace_back(SRecordedAction{SRecordedAction::eType::ACTIVATE, identity.address, std::string{monitorAddress}});

        const auto MONITOR = std::ranges::find(m_monitors, monitorAddress, &SMonitorSnapshot::address);
        ASSERT_NE(MONITOR, m_monitors.end());
        MONITOR->activeWorkspace = identity;
    }

    void removeMonitor(std::string_view address) {
        std::erase_if(m_monitors, [address](const auto& monitor) { return monitor.address == address; });
    }

    void removeWorkspace(const SWorkspaceIdentity& identity) {
        std::erase_if(m_workspaces, [&identity](const auto& workspace) { return workspace.identity == identity; });
    }

    const SWorkspaceSnapshot& workspace(const SWorkspaceIdentity& identity) const {
        const auto WORKSPACE = std::ranges::find(m_workspaces, identity, &SWorkspaceSnapshot::identity);
        EXPECT_NE(WORKSPACE, m_workspaces.end());
        return *WORKSPACE;
    }

    std::vector<SMonitorSnapshot>                                            m_monitors;
    std::vector<SWorkspaceSnapshot>                                          m_workspaces;
    std::unordered_map<std::string, SWorkspaceIdentity>                      m_configuredDefaults;
    std::unordered_map<std::string, std::vector<SDefaultWorkspaceCandidate>> m_defaultCandidates;
    std::vector<SRecordedAction>                                             m_actions;
    std::function<void()>                                                    m_onMove;
};

static SWorkspaceIdentity identity(std::string address, ::Workspace::WorkspaceID id = ::Workspace::SWorkspaceSpecialID{},
                                   ::Workspace::eWorkspaceType type = ::Workspace::eWorkspaceType::NORMAL) {
    return {
        .id      = std::move(id),
        .address = std::move(address),
        .type    = type,
    };
}

static SMonitorSnapshot monitor(std::string address, std::optional<std::string> active = std::nullopt, bool fallback = false) {
    return {
        .address         = std::move(address),
        .activeWorkspace = active ? std::optional{identity(std::move(*active))} : std::nullopt,
        .fallback        = fallback,
    };
}

static SWorkspaceSnapshot workspace(std::string address, std::optional<std::string> monitorAddress) {
    return {
        .identity       = identity(std::move(address)),
        .monitorAddress = std::move(monitorAddress),
    };
}

static SDefaultWorkspaceCandidate candidate(std::string address, std::optional<std::string> monitorAddress = std::nullopt) {
    return {
        .identity            = identity(std::move(address)),
        .boundMonitorAddress = std::move(monitorAddress),
    };
}

TEST(WorkspaceLifecyclePolicy, selectsFirstUnusedCandidateBoundToConnectingMonitor) {
    const std::vector<SWorkspaceSnapshot> WORKSPACES = {
        workspace("workspace:occupied", "monitor:left"),
    };
    const std::vector<SDefaultWorkspaceCandidate> CANDIDATES = {
        candidate("workspace:occupied"),
        candidate("workspace:other", "monitor:right"),
        candidate("workspace:left", "monitor:left"),
    };

    EXPECT_EQ(CMonitorLifecyclePolicy::selectDefaultWorkspace("monitor:left", std::nullopt, CANDIDATES, WORKSPACES), identity("workspace:left"));
}

TEST(WorkspaceLifecyclePolicy, configuredDefaultMovesExistingWorkspaceAndActivatesIt) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    context.m_monitors = {
        monitor("monitor:left", "workspace:left"),
        monitor("monitor:right"),
    };
    context.m_workspaces = {
        workspace("workspace:left", "monitor:left"),
        workspace("development", "monitor:left"),
    };
    context.m_configuredDefaults.insert_or_assign("monitor:right", identity("development"));

    policy.monitorConnected(context.m_monitors[1], context);

    EXPECT_EQ(context.m_actions,
              (std::vector<SRecordedAction>{
                  {SRecordedAction::eType::MOVE, "development", "monitor:right"},
                  {SRecordedAction::eType::ACTIVATE, "development", "monitor:right"},
              }));
}

TEST(WorkspaceLifecyclePolicy, generatedDefaultIsCreatedAndActivated) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    context.m_monitors                          = {monitor("monitor:left")};
    context.m_defaultCandidates["monitor:left"] = {
        candidate("workspace:first"),
    };

    policy.monitorConnected(context.m_monitors.front(), context);

    EXPECT_EQ(context.m_actions,
              (std::vector<SRecordedAction>{
                  {SRecordedAction::eType::CREATE, "workspace:first", "monitor:left"},
                  {SRecordedAction::eType::ACTIVATE, "workspace:first", "monitor:left"},
              }));
}

TEST(WorkspaceLifecyclePolicy, disconnectRemembersActiveAddressAndReconnectRestoresIt) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    context.m_monitors = {
        monitor("monitor:left", "workspace:left"),
        monitor("monitor:right", "workspace:right"),
    };
    context.m_workspaces = {
        workspace("workspace:left", "monitor:left"),
        workspace("workspace:right", "monitor:right"),
    };

    policy.monitorDisconnected(context.m_monitors.front(), context);

    EXPECT_EQ(policy.rememberedActiveWorkspace("monitor:left"), identity("workspace:left"));
    EXPECT_EQ(policy.returnMonitorAddress(identity("workspace:left")), "monitor:left");
    EXPECT_EQ(context.workspace(identity("workspace:left")).monitorAddress, "monitor:right");

    context.removeMonitor("monitor:left");
    context.m_monitors.emplace_back(monitor("monitor:left"));
    context.m_actions.clear();
    policy.monitorConnected(context.m_monitors.back(), context);

    EXPECT_EQ(context.workspace(identity("workspace:left")).monitorAddress, "monitor:left");
    EXPECT_FALSE(policy.returnMonitorAddress(identity("workspace:left")).has_value());
    EXPECT_EQ(context.m_actions,
              (std::vector<SRecordedAction>{
                  {SRecordedAction::eType::MOVE, "workspace:left", "monitor:left"},
                  {SRecordedAction::eType::ACTIVATE, "workspace:left", "monitor:left"},
              }));
}

TEST(WorkspaceLifecyclePolicy, reconnectToleratesWorkspaceDestructionDuringMove) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    const auto              WORKSPACE = identity("workspace:left");
    context.m_monitors                = {
        monitor("monitor:left", "workspace:left"),
        monitor("monitor:right", "workspace:right"),
    };
    context.m_workspaces = {
        workspace("workspace:left", "monitor:left"),
        workspace("workspace:right", "monitor:right"),
    };

    policy.monitorDisconnected(context.m_monitors.front(), context);
    context.removeMonitor("monitor:left");
    context.m_monitors.emplace_back(monitor("monitor:left"));
    context.m_actions.clear();
    context.m_onMove = [&] { policy.workspaceDestroyed(WORKSPACE); };

    policy.monitorConnected(context.m_monitors.back(), context);

    EXPECT_FALSE(policy.returnMonitorAddress(WORKSPACE).has_value());
    EXPECT_FALSE(policy.rememberedActiveWorkspace("monitor:left").has_value());
    EXPECT_EQ(context.m_actions, (std::vector<SRecordedAction>{{SRecordedAction::eType::MOVE, "workspace:left", "monitor:left"}}));
}

TEST(WorkspaceLifecyclePolicy, fallbackRecoveryPreservesRealMonitorProvenance) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    context.m_monitors   = {monitor("monitor:real", "name:active")};
    context.m_workspaces = {workspace("name:active", "monitor:real")};

    policy.monitorDisconnected(context.m_monitors.front(), context);
    context.removeMonitor("monitor:real");
    context.m_monitors.emplace_back(monitor("monitor:fallback", std::nullopt, true));
    context.m_configuredDefaults.insert_or_assign("monitor:fallback", identity("name:active"));
    policy.monitorConnected(context.m_monitors.back(), context);

    EXPECT_EQ(context.workspace(identity("name:active")).monitorAddress, "monitor:fallback");
    EXPECT_EQ(policy.returnMonitorAddress(identity("name:active")), "monitor:real");

    context.m_monitors.emplace_back(monitor("monitor:real"));
    context.m_actions.clear();
    policy.monitorConnected(context.m_monitors.back(), context);

    EXPECT_EQ(context.workspace(identity("name:active")).monitorAddress, "monitor:real");
    EXPECT_FALSE(policy.returnMonitorAddress(identity("name:active")).has_value());
    EXPECT_EQ(context.m_actions,
              (std::vector<SRecordedAction>{
                  {SRecordedAction::eType::MOVE, "name:active", "monitor:real"},
                  {SRecordedAction::eType::ACTIVATE, "name:active", "monitor:real"},
              }));
}

TEST(WorkspaceLifecyclePolicy, cascadedDisconnectsReturnEachWorkspaceToItsFirstOwner) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    context.m_monitors = {
        monitor("monitor:left", "workspace:left-active"),
        monitor("monitor:right", "workspace:right-active"),
    };
    context.m_workspaces = {
        workspace("workspace:left-active", "monitor:left"),
        workspace("workspace:left-extra", "monitor:left"),
        workspace("workspace:right-active", "monitor:right"),
    };

    policy.monitorDisconnected(context.m_monitors.front(), context);
    context.removeMonitor("monitor:left");
    policy.monitorDisconnected(context.m_monitors.front(), context);

    EXPECT_EQ(policy.returnMonitorAddress(identity("workspace:left-active")), "monitor:left");
    EXPECT_EQ(policy.returnMonitorAddress(identity("workspace:left-extra")), "monitor:left");
    EXPECT_EQ(policy.returnMonitorAddress(identity("workspace:right-active")), "monitor:right");

    context.removeMonitor("monitor:right");
    context.m_monitors.emplace_back(monitor("monitor:fallback", std::nullopt, true));
    context.m_defaultCandidates["monitor:fallback"] = {candidate("workspace:fallback-default")};
    policy.monitorConnected(context.m_monitors.back(), context);

    EXPECT_EQ(context.workspace(identity("workspace:left-active")).monitorAddress, "monitor:fallback");
    EXPECT_EQ(context.workspace(identity("workspace:left-extra")).monitorAddress, "monitor:fallback");
    EXPECT_EQ(context.workspace(identity("workspace:right-active")).monitorAddress, "monitor:fallback");
    EXPECT_EQ(policy.returnMonitorAddress(identity("workspace:left-active")), "monitor:left");
    EXPECT_EQ(policy.returnMonitorAddress(identity("workspace:right-active")), "monitor:right");

    context.m_monitors.emplace_back(monitor("monitor:right"));
    context.m_defaultCandidates["monitor:right"] = {candidate("workspace:right-default")};
    policy.monitorConnected(context.m_monitors.back(), context);

    EXPECT_EQ(context.workspace(identity("workspace:right-active")).monitorAddress, "monitor:right");
    EXPECT_EQ(context.workspace(identity("workspace:left-active")).monitorAddress, "monitor:fallback");
    EXPECT_FALSE(policy.returnMonitorAddress(identity("workspace:right-active")).has_value());

    context.m_monitors.emplace_back(monitor("monitor:left"));
    context.m_defaultCandidates["monitor:left"] = {candidate("workspace:left-default")};
    policy.monitorConnected(context.m_monitors.back(), context);

    EXPECT_EQ(context.workspace(identity("workspace:left-active")).monitorAddress, "monitor:left");
    EXPECT_EQ(context.workspace(identity("workspace:left-extra")).monitorAddress, "monitor:left");
    EXPECT_FALSE(policy.returnMonitorAddress(identity("workspace:left-active")).has_value());
    EXPECT_FALSE(policy.returnMonitorAddress(identity("workspace:left-extra")).has_value());
    EXPECT_EQ(context.m_monitors.back().activeWorkspace, identity("workspace:left-active"));

    const auto FALLBACK = context.m_monitors.front();
    policy.monitorDisconnected(FALLBACK, context);
    EXPECT_FALSE(policy.returnMonitorAddress(identity("workspace:fallback-default")).has_value());
}

TEST(WorkspaceLifecyclePolicy, destroyedWorkspaceForgetsProvenanceAndRememberedActivity) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    context.m_monitors   = {monitor("monitor:left", "workspace:left")};
    context.m_workspaces = {workspace("workspace:left", "monitor:left")};

    policy.monitorDisconnected(context.m_monitors.front(), context);
    policy.workspaceDestroyed(identity("workspace:left"));

    EXPECT_FALSE(policy.returnMonitorAddress(identity("workspace:left")).has_value());
    EXPECT_FALSE(policy.rememberedActiveWorkspace("monitor:left").has_value());

    context.removeWorkspace(identity("workspace:left"));
    context.removeMonitor("monitor:left");
    context.m_monitors.emplace_back(monitor("monitor:left"));
    context.m_defaultCandidates["monitor:left"] = {candidate("workspace:default")};
    context.m_actions.clear();
    policy.monitorConnected(context.m_monitors.back(), context);

    EXPECT_EQ(context.m_actions,
              (std::vector<SRecordedAction>{
                  {SRecordedAction::eType::CREATE, "workspace:default", "monitor:left"},
                  {SRecordedAction::eType::ACTIVATE, "workspace:default", "monitor:left"},
              }));
}

TEST(WorkspaceLifecyclePolicy, identityChangeMigratesProvenanceAndRememberedActivity) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    context.m_monitors   = {monitor("monitor:left", "1"), monitor("monitor:right")};
    context.m_workspaces = {workspace("1", "monitor:left")};

    policy.monitorDisconnected(context.m_monitors.front(), context);
    policy.workspaceIdentityChanged(identity("1"), identity("2"));

    EXPECT_FALSE(policy.returnMonitorAddress(identity("1")).has_value());
    EXPECT_EQ(policy.returnMonitorAddress(identity("2")), "monitor:left");
    EXPECT_EQ(policy.rememberedActiveWorkspace("monitor:left"), identity("2"));
}

TEST(WorkspaceLifecyclePolicy, provenanceDistinguishesTypedIdentityCollisions) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    const auto              NAMED    = identity("1");
    const auto              NUMBERED = identity("1", ::Workspace::SWorkspaceNumberedID{1});

    context.m_monitors   = {monitor("monitor:left", "1"), monitor("monitor:right")};
    context.m_workspaces = {
        {.identity = NAMED, .monitorAddress = "monitor:left"},
        {.identity = NUMBERED, .monitorAddress = "monitor:left"},
    };

    policy.monitorDisconnected(context.m_monitors.front(), context);
    policy.workspaceDestroyed(NAMED);

    EXPECT_FALSE(policy.returnMonitorAddress(NAMED).has_value());
    EXPECT_EQ(policy.returnMonitorAddress(NUMBERED), "monitor:left");
}

TEST(WorkspaceLifecyclePolicy, provenanceDistinguishesNamedAndSpecialAddressCollisions) {
    CMonitorLifecyclePolicy policy;
    CFakePolicyContext      context;
    const auto              NAMED   = identity("special:term");
    const auto              SPECIAL = identity("special:term", ::Workspace::SWorkspaceSpecialID{}, ::Workspace::eWorkspaceType::SPECIAL);

    context.m_monitors   = {monitor("monitor:left", "special:term"), monitor("monitor:right")};
    context.m_workspaces = {
        {.identity = NAMED, .monitorAddress = "monitor:left"},
        {.identity = SPECIAL, .monitorAddress = "monitor:left"},
    };

    policy.monitorDisconnected(context.m_monitors.front(), context);
    policy.workspaceDestroyed(NAMED);

    EXPECT_FALSE(policy.returnMonitorAddress(NAMED).has_value());
    EXPECT_EQ(policy.returnMonitorAddress(SPECIAL), "monitor:left");
}
