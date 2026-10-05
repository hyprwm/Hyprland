#include <render/Context.hpp>
#include <render/SceneResources.hpp>
#include <render/Framebuffer.hpp>

#include <gtest/gtest.h>

using namespace Render;

TEST(CapturePrivacy, OnlyExplicitCaptureSessionsFilterPrivateContent) {
    for (const bool isolated : {false, true}) {
        CRenderContext ctx;
        auto           resources = isolated ? makeShared<CSceneResources>(SP<IFramebuffer>{}) : makeShared<CSceneResources>(PHLMONITORREF{});
        ASSERT_TRUE(ctx.begin(resources));
        EXPECT_EQ(ctx.readOnlyEffects(), isolated);
        EXPECT_FALSE(ctx.m_renderingCapture);

        for (const bool capture : {false, true}) {
            for (const bool snapshot : {false, true}) {
                for (const bool feedbackBlocked : {false, true}) {
                    ctx.m_renderingCapture     = capture;
                    ctx.m_renderingSnapshot    = snapshot;
                    ctx.m_blockSurfaceFeedback = feedbackBlocked;
                    EXPECT_TRUE(ctx.shouldRenderContent(false));
                    EXPECT_EQ(ctx.shouldRenderContent(true), !capture);
                }
            }
        }
    }
}

TEST(CapturePrivacy, ResetAndReuseDoNotInheritCapturePrivacy) {
    CRenderContext ctx;
    EXPECT_FALSE(ctx.m_renderingCapture);
    ASSERT_TRUE(ctx.begin());
    ctx.m_renderingCapture = true;
    EXPECT_FALSE(ctx.shouldRenderContent(true));
    ctx.reset();
    EXPECT_FALSE(ctx.m_renderingCapture);
    EXPECT_TRUE(ctx.shouldRenderContent(true));
    ASSERT_TRUE(ctx.begin());
    EXPECT_FALSE(ctx.m_renderingCapture);
    EXPECT_TRUE(ctx.shouldRenderContent(true));
}
