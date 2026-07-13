#include <config/shared/workspace/WorkspaceRuleManager.hpp>
#include <output/IMonitorIdentifiable.hpp>

#include <gtest/gtest.h>
#include <hyprutils/string/String.hpp>

#include <string>

class CWorkspaceRuleTestMonitor : public Monitor::IMonitorIdentifiable {
  public:
    virtual MONITORID id() const override {
        return 0;
    }

    virtual std::string_view name() const override {
        return m_name;
    }

    virtual std::string_view description() const override {
        return m_description;
    }

    virtual std::string_view shortDescription() const override {
        return m_shortDescription;
    }

    virtual bool matchesStaticSelector(std::string_view selector) const override {
        if (selector.starts_with("desc:")) {
            const auto DESCRIPTION = Hyprutils::String::trim(selector.substr(5));
            return m_description.starts_with(DESCRIPTION) || m_shortDescription.starts_with(DESCRIPTION);
        }

        return selector == m_name;
    }

    std::string m_name;
    std::string m_description;
    std::string m_shortDescription;
};

static Config::CWorkspaceRule defaultWorkspaceRule(const std::string& workspace, const std::string& monitor) {
    Config::CWorkspaceRule rule;
    rule.m_workspaceString = workspace;
    rule.m_workspaceName   = workspace;
    rule.m_monitor         = monitor;
    rule.m_isDefault       = true;
    return rule;
}

TEST(WorkspaceRuleManager, defaultWorkspaceMatchesMonitorName) {
    Config::CWorkspaceRuleManager manager;
    manager.add(defaultWorkspaceRule("4", "DP-1"));

    CWorkspaceRuleTestMonitor monitor;
    monitor.m_name = "DP-1";

    EXPECT_EQ(manager.getDefaultWorkspaceFor(monitor), "4");
}

TEST(WorkspaceRuleManager, defaultWorkspaceMatchesMonitorDescription) {
    Config::CWorkspaceRuleManager manager;
    manager.add(defaultWorkspaceRule("4", "desc:Microstep MPG321UX OLED"));

    CWorkspaceRuleTestMonitor monitor;
    monitor.m_name        = "DP-3";
    monitor.m_description = "Microstep MPG321UX OLED 0x01010101";

    EXPECT_EQ(manager.getDefaultWorkspaceFor(monitor), "4");
}

TEST(WorkspaceRuleManager, defaultWorkspaceMatchesMonitorShortDescription) {
    Config::CWorkspaceRuleManager manager;
    manager.add(defaultWorkspaceRule("5", "desc:ASUSTek COMPUTER INC PA24ACRV"));

    CWorkspaceRuleTestMonitor monitor;
    monitor.m_name             = "DP-2";
    monitor.m_shortDescription = "ASUSTek COMPUTER INC PA24ACRV S5LMYX000514";

    EXPECT_EQ(manager.getDefaultWorkspaceFor(monitor), "5");
}

TEST(WorkspaceRuleManager, defaultWorkspaceSkipsNonMatchingMonitor) {
    Config::CWorkspaceRuleManager manager;
    manager.add(defaultWorkspaceRule("4", "desc:Microstep MPG321UX OLED"));

    CWorkspaceRuleTestMonitor monitor;
    monitor.m_name        = "DP-1";
    monitor.m_description = "ASUSTek COMPUTER INC PA24ACRV S1LMYX001494";

    EXPECT_EQ(manager.getDefaultWorkspaceFor(monitor), "");
}

TEST(WorkspaceRuleManager, disabledDefaultWorkspaceIsSkipped) {
    Config::CWorkspaceRuleManager manager;
    auto                          rule = manager.add(defaultWorkspaceRule("4", "DP-1"));
    rule->setEnabled(false);

    CWorkspaceRuleTestMonitor monitor;
    monitor.m_name = "DP-1";

    EXPECT_EQ(manager.getDefaultWorkspaceFor(monitor), "");
}

TEST(WorkspaceRuleManager, replaceOrAddKeepsExistingSharedRule) {
    Config::CWorkspaceRule first = defaultWorkspaceRule("4", "DP-1");
    first.m_isPersistent         = true;

    Config::CWorkspaceRule second = defaultWorkspaceRule("4", "DP-2");
    second.m_isPersistent         = false;

    Config::CWorkspaceRuleManager manager;
    const auto                    firstPtr  = manager.replaceOrAdd(std::move(first));
    const auto                    secondPtr = manager.replaceOrAdd(std::move(second));

    EXPECT_EQ(firstPtr, secondPtr);
    EXPECT_EQ(firstPtr->m_monitor, "DP-2");
    EXPECT_FALSE(firstPtr->m_isPersistent.value_or(true));
}
