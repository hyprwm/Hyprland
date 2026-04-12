#include <desktop/Workspace.hpp>
#include <config/ConfigManager.hpp>

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
    // base scale below the 0.1 threshold is treated as unset
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(0.0f, 0.0f, 1.5f), 1.5f);
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(0.0f, 0.05f, 2.0f), 2.0f);
}

TEST(WorkspaceScale, ResolveScaleNegativeTargetTreatedAsUnset) {
    EXPECT_FLOAT_EQ(CWorkspace::resolveScale(-1.0f, 1.5f, 1.0f), 1.5f);
}

// CConfigManager::mergeWorkspaceRules — scale merge semantics.

TEST(WorkspaceScale, MergeRulesScaleOverride) {
    SWorkspaceRule a;
    a.workspaceString = "1";
    a.xwaylandScale= 1.5f;

    SWorkspaceRule b;
    b.workspaceString = "1";
    b.xwaylandScale= 2.0f;

    const auto merged = CConfigManager::mergeWorkspaceRules(a, b);
    ASSERT_TRUE(merged.xwaylandScale.has_value());
    EXPECT_FLOAT_EQ(merged.xwaylandScale.value(), 2.0f);
}

TEST(WorkspaceScale, MergeRulesScalePreservedWhenSecondUnset) {
    SWorkspaceRule a;
    a.workspaceString = "1";
    a.xwaylandScale= 1.5f;

    SWorkspaceRule b;
    b.workspaceString = "1";

    const auto merged = CConfigManager::mergeWorkspaceRules(a, b);
    ASSERT_TRUE(merged.xwaylandScale.has_value());
    EXPECT_FLOAT_EQ(merged.xwaylandScale.value(), 1.5f);
}

TEST(WorkspaceScale, MergeRulesScaleUnsetWhenBothUnset) {
    SWorkspaceRule a;
    a.workspaceString = "1";
    SWorkspaceRule b;
    b.workspaceString = "1";

    const auto merged = CConfigManager::mergeWorkspaceRules(a, b);
    EXPECT_FALSE(merged.xwaylandScale.has_value());
}

TEST(WorkspaceScale, MergeRulesScaleSetFromSecondOnly) {
    SWorkspaceRule a;
    a.workspaceString = "1";
    SWorkspaceRule b;
    b.workspaceString = "1";
    b.xwaylandScale= 1.75f;

    const auto merged = CConfigManager::mergeWorkspaceRules(a, b);
    ASSERT_TRUE(merged.xwaylandScale.has_value());
    EXPECT_FLOAT_EQ(merged.xwaylandScale.value(), 1.75f);
}

// Pure formula test for the X11 effective scale logic in CWindow::effectiveX11Scale.
// We replicate the formula (base^2 / target) here so the math is verified in isolation
// without needing to construct a CWindow (which depends on the full compositor).

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
    // A fullscreen X11 window at 7680x2160 with base scale 1.6666 and workspace
    // target 3.0 should render at roughly 55.5% of the pixel count, yielding a
    // 1.8x upscale (3.0 / 1.6666) applied by the renderer.
    const float base   = 1.6666f;
    const float target = 3.0f;
    const float eff    = effectiveX11ScaleFormula(base, target);
    // logical_size * eff is what the X11 window gets; desired ratio is target/base
    const float logicalPixels  = 4608.f; // 7680 / 1.6666
    const float x11Pixels      = logicalPixels * eff;
    const float monitorPx      = logicalPixels * base;
    const float upscaleApplied = monitorPx / x11Pixels;
    EXPECT_NEAR(upscaleApplied, target / base, 1e-4f);
}

TEST(WorkspaceScale, X11FormulaBase1Target2Gives0Point5) {
    // Simplest case: monitor base scale 1.0, workspace target 2.0 → effective 0.5.
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(1.0f, 2.0f), 0.5f);
}

TEST(WorkspaceScale, X11FormulaIdentityWhenTargetEqualsBase) {
    // If the workspace target happens to equal the monitor base, the effective scale
    // collapses back to the base (no visible change).
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(1.5f, 1.5f), 1.5f);
    EXPECT_FLOAT_EQ(effectiveX11ScaleFormula(2.0f, 2.0f), 2.0f);
}
