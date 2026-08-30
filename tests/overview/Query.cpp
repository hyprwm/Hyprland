#include <overview/hyprland/Query.hpp>

#include <gtest/gtest.h>

using namespace Overview::Hyprland;
using namespace Overview::Hyprland::Mode;

TEST(OverviewQuery, selectsPrefixedModes) {
    const CQuery WINDOW_QUERY{"/firefox"};
    const CQuery WORKSPACE_QUERY{".coding"};

    EXPECT_EQ(WINDOW_QUERY.mode(), eQueryMode::WINDOW);
    EXPECT_EQ(WINDOW_QUERY.term(), "firefox");
    EXPECT_EQ(WORKSPACE_QUERY.mode(), eQueryMode::WORKSPACE);
    EXPECT_EQ(WORKSPACE_QUERY.term(), "coding");
}

TEST(OverviewQuery, resolvesModeForProvidedQuery) {
    const CQuery QUERY{"unprefixed"};

    EXPECT_EQ(QUERY.mode("/firefox"), eQueryMode::WINDOW);
    EXPECT_EQ(QUERY.mode(".coding"), eQueryMode::WORKSPACE);
    EXPECT_EQ(QUERY.mode("unprefixed"), eQueryMode::ALL);
}

TEST(OverviewQuery, supportsCustomPrefixes) {
    const SQueryConfig CONFIG{.windowPrefix = '!', .workspacePrefix = '@'};
    const CQuery       WINDOW_QUERY{"!firefox", CONFIG};
    const CQuery       WORKSPACE_QUERY{"@coding", CONFIG};
    const CQuery       OLD_PREFIX{"/firefox", CONFIG};

    EXPECT_EQ(WINDOW_QUERY.mode(), eQueryMode::WINDOW);
    EXPECT_EQ(WINDOW_QUERY.term(), "firefox");
    EXPECT_EQ(WORKSPACE_QUERY.mode(), eQueryMode::WORKSPACE);
    EXPECT_EQ(WORKSPACE_QUERY.term(), "coding");
    EXPECT_EQ(OLD_PREFIX.mode(), eQueryMode::ALL);
    EXPECT_EQ(OLD_PREFIX.term(), "/firefox");
}

TEST(OverviewQuery, usesConfiguredDefaultWithoutStrippingUnknownPrefix) {
    const CQuery QUERY{"?coding", SQueryConfig{.defaultMode = eQueryMode::WORKSPACE}};

    EXPECT_EQ(QUERY.mode(), eQueryMode::WORKSPACE);
    EXPECT_EQ(QUERY.term(), "?coding");
}

TEST(OverviewQuery, supportsEveryDefaultMode) {
    EXPECT_EQ(CQuery("term", SQueryConfig{.defaultMode = eQueryMode::ALL}).mode(), eQueryMode::ALL);
    EXPECT_EQ(CQuery("term", SQueryConfig{.defaultMode = eQueryMode::WINDOW}).mode(), eQueryMode::WINDOW);
    EXPECT_EQ(CQuery("term", SQueryConfig{.defaultMode = eQueryMode::WORKSPACE}).mode(), eQueryMode::WORKSPACE);
}

TEST(OverviewQuery, resolvesDuplicatePrefixesInRegistrationOrder) {
    const CQuery QUERY{"/term", SQueryConfig{.windowPrefix = '/', .workspacePrefix = '/'}};

    EXPECT_EQ(QUERY.mode(), eQueryMode::WINDOW);
    EXPECT_EQ(QUERY.term(), "term");
}

TEST(OverviewQuery, emptyEffectiveQueryDoesNotFilterOrMatchWindows) {
    const CQuery QUERY{"/"};

    EXPECT_TRUE(QUERY.empty());
    EXPECT_EQ(QUERY.matchWorkspace("anything"), eWorkspaceMatch::MATCH);
    EXPECT_FALSE(QUERY.matchesWindow("anything", "anything"));
}

TEST(OverviewQuery, windowModeMatchesOnlyWindowClassAndTitle) {
    const CQuery QUERY{"/fire"};
    bool         selectorCalled = false;

    EXPECT_TRUE(QUERY.matchesWindow("Firefox", "New Tab"));
    EXPECT_TRUE(QUERY.matchesWindow("org.example.App", "Wildfire"));
    EXPECT_FALSE(QUERY.matchesWindow("org.example.App", "New Tab"));
    EXPECT_EQ(QUERY.matchWorkspace("fire",
                                   [&](std::string_view) {
                                       selectorCalled = true;
                                       return true;
                                   }),
              eWorkspaceMatch::NONE);
    EXPECT_FALSE(selectorCalled);
    EXPECT_TRUE(QUERY.usesWindowMetadata());
}

TEST(OverviewQuery, workspaceModeMatchesNamesAndExactNames) {
    const CQuery QUERY{".coding"};

    EXPECT_EQ(QUERY.matchWorkspace("Main Coding"), eWorkspaceMatch::MATCH);
    EXPECT_EQ(QUERY.matchWorkspace("CoDiNg"), eWorkspaceMatch::EXACT);
    EXPECT_EQ(QUERY.matchWorkspace("gaming"), eWorkspaceMatch::NONE);
    EXPECT_FALSE(QUERY.matchesWindow("coding", "coding"));
    EXPECT_FALSE(QUERY.usesWindowMetadata());
}

TEST(OverviewQuery, workspaceModeDelegatesStaticSelectors) {
    const CQuery QUERY{".m[DP-1]"};
    bool         selectorCalled = false;

    const auto   MATCH = QUERY.matchWorkspace("coding", [&](std::string_view selector) {
        selectorCalled = true;
        EXPECT_EQ(selector, "m[DP-1]");
        return true;
    });

    EXPECT_TRUE(selectorCalled);
    EXPECT_EQ(MATCH, eWorkspaceMatch::MATCH);
}

TEST(OverviewQuery, workspaceModeRecognizesWhitespaceWrappedSelectors) {
    for (const auto& raw : {". name:coding ", ". 1 ", ". m[DP-1] "}) {
        const CQuery QUERY{raw};
        bool         selectorCalled = false;

        EXPECT_EQ(QUERY.matchWorkspace("unrelated",
                                       [&](std::string_view) {
                                           selectorCalled = true;
                                           return true;
                                       }),
                  eWorkspaceMatch::MATCH);
        EXPECT_TRUE(selectorCalled);
    }
}

TEST(OverviewQuery, workspaceModeRejectsUnmatchedAndMalformedSelectors) {
    const CQuery QUERY{".m[DP-1"};
    bool         selectorCalled = false;

    EXPECT_EQ(QUERY.matchWorkspace("coding",
                                   [&](std::string_view selector) {
                                       selectorCalled = true;
                                       EXPECT_EQ(selector, "m[DP-1");
                                       return false;
                                   }),
              eWorkspaceMatch::NONE);
    EXPECT_TRUE(selectorCalled);
}

TEST(OverviewQuery, plainWorkspaceSearchDoesNotInvokeSelectorParser) {
    const CQuery QUERY{".missing"};
    bool         selectorCalled = false;

    EXPECT_EQ(QUERY.matchWorkspace("coding",
                                   [&](std::string_view) {
                                       selectorCalled = true;
                                       return true;
                                   }),
              eWorkspaceMatch::NONE);
    EXPECT_FALSE(selectorCalled);
}

TEST(OverviewQuery, combinedModePreservesExistingMatching) {
    const CQuery QUERY{"fire"};

    EXPECT_EQ(QUERY.matchWorkspace("Wildfire"), eWorkspaceMatch::MATCH);
    EXPECT_EQ(QUERY.matchWorkspace("FiRe"), eWorkspaceMatch::EXACT);
    EXPECT_TRUE(QUERY.matchesWindow("Firefox", "New Tab"));
}
