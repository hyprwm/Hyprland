#include <desktop/Workspace.hpp>
#include <config/shared/workspace/WorkspaceRule.hpp>

#include <gtest/gtest.h>

// CWorkspace::resolveScale — pure scale fallback logic.

TEST(WorkspaceScale, ResolveScalePrefersTargetWhenSet) {
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(2.0f, 1.0f, 1.5f), 2.0f);
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(1.25f, 2.0f, 1.0f), 1.25f);
}

TEST(WorkspaceScale, ResolveScaleFallsBackToBaseWhenNoTarget) {
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(0.0f, 1.6666f, 1.0f), 1.6666f);
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(0.0f, 2.0f, 1.5f), 2.0f);
}

TEST(WorkspaceScale, ResolveScaleFallsBackToDefaultWhenBaseInvalid) {
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(0.0f, 0.0f, 1.5f), 1.5f);
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(0.0f, 0.05f, 2.0f), 2.0f);
}

TEST(WorkspaceScale, ResolveScaleNegativeTargetTreatedAsUnset) {
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(-1.0f, 1.5f, 1.0f), 1.5f);
}

// CWorkspaceRule::mergeLeft — xwaylandScale merge semantics.

TEST(WorkspaceScale, MergeRulesScaleOverride) {
    Config::CWorkspaceRule a;
    a.m_xwaylandScale = 1.5f;
    Config::CWorkspaceRule b;
    b.m_xwaylandScale = 2.0f;

    a.mergeLeft(b);
    ASSERT_TRUE(a.m_xwaylandScale.has_value());
    EXPECT_FLOAT_EQ(a.m_xwaylandScale.value(), 2.0f);
}

TEST(WorkspaceScale, MergeRulesScalePreservedWhenOtherUnset) {
    Config::CWorkspaceRule a;
    a.m_xwaylandScale = 1.5f;
    Config::CWorkspaceRule b;

    a.mergeLeft(b);
    ASSERT_TRUE(a.m_xwaylandScale.has_value());
    EXPECT_FLOAT_EQ(a.m_xwaylandScale.value(), 1.5f);
}

TEST(WorkspaceScale, MergeRulesScaleUnsetWhenBothUnset) {
    Config::CWorkspaceRule a;
    Config::CWorkspaceRule b;

    a.mergeLeft(b);
    EXPECT_FALSE(a.m_xwaylandScale.has_value());
}

TEST(WorkspaceScale, MergeRulesScaleSetFromOtherOnly) {
    Config::CWorkspaceRule a;
    Config::CWorkspaceRule b;
    b.m_xwaylandScale = 1.75f;

    a.mergeLeft(b);
    ASSERT_TRUE(a.m_xwaylandScale.has_value());
    EXPECT_FLOAT_EQ(a.m_xwaylandScale.value(), 1.75f);
}

// Pure formula test for the X11 effective scale logic in CWindow::effectiveX11Scale.

static float effectiveX11ScaleFormula(float base, float workspaceTargetScale) {
    if (workspaceTargetScale <= 0.f)
        return base;
    return base * base / workspaceTargetScale;
}

TEST(WorkspaceScale, X11FormulaNoOverrideReturnsBase) {
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(1.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(1.6666f, 0.0f), 1.6666f);
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(2.0f, -1.0f), 2.0f);
}

TEST(WorkspaceScale, X11FormulaProducesUpscaleRatio) {
    const float base   = 1.6666f;
    const float target = 3.0f;
    const float eff    = effectiveX11ScaleFormula(base, target);
    const float logicalPixels  = 4608.f;
    const float x11Pixels      = logicalPixels * eff;
    const float monitorPx      = logicalPixels * base;
    const float upscaleApplied = monitorPx / x11Pixels;
    EXPECT_NEAR(upscaleApplied, target / base, 1e-4f);
}

TEST(WorkspaceScale, X11FormulaBase1Target2Gives0Point5) {
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(1.0f, 2.0f), 0.5f);
}

TEST(WorkspaceScale, X11FormulaIdentityWhenTargetEqualsBase) {
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(1.5f, 1.5f), 1.5f);
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(2.0f, 2.0f), 2.0f);
}
