#include <render/pass/Pass.hpp>
#include <render/types.hpp>

#include <gtest/gtest.h>

TEST(Render, renderingContextUsesExplicitPassAndFreshRootState) {
    Render::CRenderPass       pass;
    Render::CRenderingContext context{{}, pass};

    EXPECT_EQ(&context.renderPass(), &pass);
    EXPECT_FALSE(context.sceneMonitor);
    EXPECT_FALSE(context.outputMonitor);
    ASSERT_TRUE(context.cmSettingsCache);
    EXPECT_TRUE(context.cmSettingsCache->entries.empty());

    EXPECT_EQ(context.fbSize, Vector2D(-1, -1));
    EXPECT_EQ(context.projectionType, Render::RPT_MONITOR);
    EXPECT_FALSE(context.currentFB);
    EXPECT_FALSE(context.mainFB);
    EXPECT_FALSE(context.outFB);
    EXPECT_TRUE(context.damage.empty());
    EXPECT_TRUE(context.finalDamage.empty());
    EXPECT_FLOAT_EQ(context.mouseZoomFactor, 1.F);
    EXPECT_TRUE(context.mouseZoomUseMouse);
    EXPECT_FALSE(context.useNearestNeighbor);
    EXPECT_TRUE(context.transformDamage);
    EXPECT_FALSE(context.noSimplify);
    EXPECT_FALSE(context.renderingTransformedSource);
    EXPECT_FALSE(context.isolatedWorkspace);
    EXPECT_FALSE(context.isolatedWorkspaceFullScene);
    EXPECT_FALSE(context.blockSurfaceFeedback);
    EXPECT_FALSE(context.renderingSnapshot);
    EXPECT_FALSE(context.precomputeBlur);
    EXPECT_TRUE(context.updatesMonitorBlurState);
    EXPECT_EQ(context.renderMode, Render::RENDER_MODE_NORMAL);
    EXPECT_FALSE(context.fakeFrame);
    EXPECT_FALSE(context.offloadedFramebuffer);
    EXPECT_FALSE(context.applyFinalScreenShader);
    EXPECT_FALSE(context.buffer);
    EXPECT_FALSE(context.renderbuffer);
}

TEST(Render, childRenderingContextSharesResourcesWithDistinctPass) {
    Render::CRenderPass       parentPass;
    Render::CRenderPass       childPass;
    Render::CRenderingContext parent{{}, parentPass};
    parent.noSimplify        = true;
    parent.blockScreenShader = true;
    parent.renderingSnapshot = true;
    parent.precomputeBlur    = true;
    parent.renderMode        = Render::RENDER_MODE_TO_BUFFER;
    parent.fbSize            = {1920, 1080};

    Render::CRenderingContext child{parent, childPass};

    EXPECT_EQ(&child.renderPass(), &childPass);
    EXPECT_EQ(&parent.renderPass(), &parentPass);
    EXPECT_EQ(child.outputMonitor.lock(), parent.outputMonitor.lock());
    EXPECT_EQ(child.cmSettingsCache.get(), parent.cmSettingsCache.get());
    EXPECT_FALSE(child.updatesMonitorBlurState);
    EXPECT_TRUE(child.noSimplify);
    EXPECT_TRUE(child.blockScreenShader);
    EXPECT_TRUE(child.renderingSnapshot);
    EXPECT_TRUE(child.precomputeBlur);
    EXPECT_EQ(child.renderMode, Render::RENDER_MODE_TO_BUFFER);
    EXPECT_EQ(child.fbSize, Vector2D(1920, 1080));

    child.noSimplify        = false;
    child.blockScreenShader = false;
    child.renderingSnapshot = false;
    child.precomputeBlur    = false;
    child.renderMode        = Render::RENDER_MODE_NORMAL;
    child.fbSize            = {1, 2};
    child.damage.add(CBox(1, 2, 3, 4));

    EXPECT_TRUE(parent.noSimplify);
    EXPECT_TRUE(parent.blockScreenShader);
    EXPECT_TRUE(parent.renderingSnapshot);
    EXPECT_TRUE(parent.precomputeBlur);
    EXPECT_TRUE(parent.updatesMonitorBlurState);
    EXPECT_EQ(parent.renderMode, Render::RENDER_MODE_TO_BUFFER);
    EXPECT_EQ(parent.fbSize, Vector2D(1920, 1080));
    EXPECT_TRUE(parent.damage.empty());
}
