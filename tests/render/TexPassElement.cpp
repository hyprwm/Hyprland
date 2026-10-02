#include <render/pass/TexPassElement.hpp>
#include <render/Context.hpp>

#include <gtest/gtest.h>

TEST(TexPassElement, ReportsNoBlur) {
    Render::CRenderContext ctx;
    CTexPassElement        element{CTexPassElement::SRenderData{}};

    EXPECT_FALSE(element.needsLiveBlur(ctx));
    EXPECT_FALSE(element.needsPrecomputeBlur(ctx));
}

TEST(TexPassElement, ReportsExplicitLiveBlur) {
    Render::CRenderContext ctx;
    CTexPassElement        element{CTexPassElement::SRenderData{
        .blur                  = true,
        .blockBlurOptimization = true,
    }};

    EXPECT_TRUE(element.needsLiveBlur(ctx));
    EXPECT_FALSE(element.needsPrecomputeBlur(ctx));
}

TEST(TexPassElement, LiveBlurOverrideForcesLiveBlur) {
    Render::CRenderContext ctx;
    CTexPassElement        element{CTexPassElement::SRenderData{
        .blur             = true,
        .liveBlurOverride = true,
    }};

    EXPECT_TRUE(element.needsLiveBlur(ctx));
    EXPECT_FALSE(element.needsPrecomputeBlur(ctx));
}

TEST(TexPassElement, LiveBlurOverrideForcesPrecomputedBlur) {
    Render::CRenderContext ctx;
    CTexPassElement        element{CTexPassElement::SRenderData{
        .blur             = true,
        .liveBlurOverride = false,
    }};

    EXPECT_FALSE(element.needsLiveBlur(ctx));
    EXPECT_TRUE(element.needsPrecomputeBlur(ctx));
}
