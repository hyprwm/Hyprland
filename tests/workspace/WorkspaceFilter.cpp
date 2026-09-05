#include <workspace/filter/WorkspaceFilter.hpp>

#include <workspace/AbstractWorkspace.hpp>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace Workspace;
using namespace Workspace::Filter;

class CFilterTestWorkspace final : public IAbstractWorkspace {
  public:
    CFilterTestWorkspace(WorkspaceID id, std::string displayName, std::string addressableName, eWorkspaceType type = eWorkspaceType::NORMAL) :
        IAbstractWorkspace(type), m_id(std::move(id)), m_displayName(std::move(displayName)), m_addressableName(std::move(addressableName)) {
        ;
    }

    virtual WorkspaceID id() const override {
        return m_id;
    }

    virtual const std::string& displayName() const override {
        return m_displayName;
    }

    virtual const std::string& addressableName() const override {
        return m_addressableName;
    }

    virtual SP<Monitor::IMonitorAddressable> monitor() const override {
        return {};
    }

  private:
    WorkspaceID m_id;
    std::string m_displayName;
    std::string m_addressableName;
};

static SP<IAbstractWorkspace> numbered(uint32_t id, std::string name = {}) {
    if (name.empty())
        name = std::to_string(id);

    return dynamicPointerCast<IAbstractWorkspace>(makeShared<CFilterTestWorkspace>(SWorkspaceNumberedID{id}, name, std::to_string(id)));
}

static SP<IAbstractWorkspace> named(std::string name) {
    return dynamicPointerCast<IAbstractWorkspace>(makeShared<CFilterTestWorkspace>(SWorkspaceSpecialID{}, name, name));
}

static SP<IAbstractWorkspace> special(std::string name) {
    return dynamicPointerCast<IAbstractWorkspace>(makeShared<CFilterTestWorkspace>(SWorkspaceSpecialID{}, name, "special:" + name, eWorkspaceType::SPECIAL));
}

TEST(WorkspaceFilter, emptyFilterKeepsAllCandidates) {
    CWorkspaceFilter                    filter{"  "};
    std::vector<SP<IAbstractWorkspace>> workspaces = {numbered(1), numbered(2), named("code")};

    filter.transform(workspaces);

    EXPECT_TRUE(filter.error().empty());
    EXPECT_EQ(workspaces.size(), 3);
}

TEST(WorkspaceFilter, rangeReturnsEveryCandidateInRange) {
    CWorkspaceFilter                    filter{"r[2-4]"};
    std::vector<SP<IAbstractWorkspace>> workspaces = {numbered(1), numbered(2), numbered(3), numbered(4), numbered(5)};

    filter.transform(workspaces);

    ASSERT_EQ(workspaces.size(), 3);
    EXPECT_EQ(std::get<SWorkspaceNumberedID>(workspaces[0]->id()).value, 2);
    EXPECT_EQ(std::get<SWorkspaceNumberedID>(workspaces[1]->id()).value, 3);
    EXPECT_EQ(std::get<SWorkspaceNumberedID>(workspaces[2]->id()).value, 4);
}

TEST(WorkspaceFilter, allStatementsAreAnded) {
    CWorkspaceFilter                    filter{"r[1-4] s[false] n[false]"};
    std::vector<SP<IAbstractWorkspace>> workspaces = {numbered(1), numbered(4), numbered(5), named("code"), special("term")};

    filter.transform(workspaces);

    ASSERT_EQ(workspaces.size(), 2);
    EXPECT_EQ(std::get<SWorkspaceNumberedID>(workspaces[0]->id()).value, 1);
    EXPECT_EQ(std::get<SWorkspaceNumberedID>(workspaces[1]->id()).value, 4);
}

TEST(WorkspaceFilter, directSelectorsMatchIdentity) {
    const auto NUMBERED = numbered(3, "renamed");
    const auto NAMED    = named("code");
    const auto SPECIAL  = special("term");

    EXPECT_TRUE(CWorkspaceFilter{"3"}.matches(*NUMBERED));
    EXPECT_TRUE(CWorkspaceFilter{"name:code"}.matches(*NAMED));
    EXPECT_TRUE(CWorkspaceFilter{"code"}.matches(*NAMED));
    EXPECT_TRUE(CWorkspaceFilter{"special:term"}.matches(*SPECIAL));
    EXPECT_FALSE(CWorkspaceFilter{"name:3"}.matches(*NUMBERED));
    EXPECT_FALSE(CWorkspaceFilter{"name:renamed"}.matches(*NUMBERED));
    EXPECT_FALSE(CWorkspaceFilter{"renamed"}.matches(*NUMBERED));
}

TEST(WorkspaceFilter, bareNamedSelectorMatchesAddressableName) {
    const auto WORKSPACE = makeShared<CFilterTestWorkspace>(SWorkspaceSpecialID{}, "renamed", "vaxry");

    EXPECT_TRUE(CWorkspaceFilter{"vaxry"}.matches(*WORKSPACE));
    EXPECT_TRUE(CWorkspaceFilter{"name:vaxry"}.matches(*WORKSPACE));
    EXPECT_FALSE(CWorkspaceFilter{"renamed"}.matches(*WORKSPACE));
    EXPECT_FALSE(CWorkspaceFilter{"name:renamed"}.matches(*WORKSPACE));
}

TEST(WorkspaceFilter, namedSelectorsSupportBooleanPrefixAndSuffixForms) {
    const auto CODE = named("dev-code");

    EXPECT_TRUE(CWorkspaceFilter{"n[true]"}.matches(*CODE));
    EXPECT_TRUE(CWorkspaceFilter{"n[s:dev]"}.matches(*CODE));
    EXPECT_TRUE(CWorkspaceFilter{"n[e:code]"}.matches(*CODE));
    EXPECT_FALSE(CWorkspaceFilter{"n[false]"}.matches(*CODE));
}

TEST(WorkspaceFilter, invalidNonEmptyFiltersMatchNothingAndReportErrors) {
    for (const auto& selector : {"f[", "f[]", "r[4-2]", "x[1]", "s[maybe]", "n[maybe]", "name:", "special:", "n[s:]", "n[e:]", "0", "999999999999999999999999"}) {
        CWorkspaceFilter                    filter{selector};
        std::vector<SP<IAbstractWorkspace>> workspaces = {numbered(1), named("code"), special("term")};

        filter.transform(workspaces);

        EXPECT_FALSE(filter.error().empty()) << selector;
        EXPECT_TRUE(workspaces.empty()) << selector;
    }
}

TEST(WorkspaceFilter, dynamicLegacyGrammarIsRecognizedButFailsClosed) {
    for (const auto& selector : {"m[DP-1]", "w[0]", "w[2]", "w[tpgv1-3]", "f[-1]", "f[2]"}) {
        CWorkspaceFilter                    filter{selector};
        std::vector<SP<IAbstractWorkspace>> workspaces = {numbered(1)};

        filter.transform(workspaces);

        EXPECT_TRUE(filter.error().empty()) << selector;
        EXPECT_TRUE(workspaces.empty()) << selector;
    }
}

class CFilterTestDataSource final : public IDataSource {
  public:
    bool monitorMatches(const IAbstractWorkspace&, std::string_view selector) const override {
        return selector == m_monitor;
    }

    int windowCount(const IAbstractWorkspace&, const SWindowCountOptions& options) const override {
        m_lastOptions = options;
        return m_windowCount;
    }

    int fullscreenState(const IAbstractWorkspace&) const override {
        return m_fullscreenState;
    }

    std::string                 m_monitor         = "DP-1";
    int                         m_windowCount     = 2;
    int                         m_fullscreenState = 0;
    mutable SWindowCountOptions m_lastOptions;
};

TEST(WorkspaceFilter, dynamicStatementsUseInjectedDataSource) {
    CFilterTestDataSource  dataSource;
    const auto             WORKSPACE = numbered(1);
    const CWorkspaceFilter filter{"m[DP-1]w[tpgv2]f[0]", &dataSource};

    EXPECT_TRUE(filter.matches(*WORKSPACE));
    EXPECT_TRUE(dataSource.m_lastOptions.tiled.value_or(false));
    EXPECT_TRUE(dataSource.m_lastOptions.pinned);
    EXPECT_TRUE(dataSource.m_lastOptions.groups);
    EXPECT_TRUE(dataSource.m_lastOptions.visible);
}

TEST(WorkspaceFilter, windowCountSupportsExactAndRangeSelectors) {
    CFilterTestDataSource dataSource;
    const auto            WORKSPACE = numbered(1);

    EXPECT_TRUE((CWorkspaceFilter{"w[2]", &dataSource}.matches(*WORKSPACE)));
    EXPECT_TRUE((CWorkspaceFilter{"w[1-3]", &dataSource}.matches(*WORKSPACE)));
    EXPECT_FALSE((CWorkspaceFilter{"w[0]", &dataSource}.matches(*WORKSPACE)));
    EXPECT_FALSE((CWorkspaceFilter{"w[3-5]", &dataSource}.matches(*WORKSPACE)));

    dataSource.m_windowCount = 0;
    EXPECT_TRUE((CWorkspaceFilter{"w[0]", &dataSource}.matches(*WORKSPACE)));
}
