#include <render/pass/TexPassElement.hpp>
#include <render/pass/Pass.hpp>
#include <render/types.hpp>

#include <gtest/gtest.h>

TEST(TexPassElement, ReportsNoBlur) {
    Render::CRenderPass       pass;
    Render::CRenderingContext context{{}, pass};
    CTexPassElement           element{CTexPassElement::SRenderData{}};

    EXPECT_FALSE(element.needsLiveBlur(context));
    EXPECT_FALSE(element.needsPrecomputeBlur(context));
}

TEST(TexPassElement, ReportsExplicitLiveBlur) {
    Render::CRenderPass       pass;
    Render::CRenderingContext context{{}, pass};
    CTexPassElement           element{CTexPassElement::SRenderData{
        .blur                  = true,
        .blockBlurOptimization = true,
    }};

    EXPECT_TRUE(element.needsLiveBlur(context));
    EXPECT_FALSE(element.needsPrecomputeBlur(context));
}

TEST(TexPassElement, LiveBlurOverrideForcesLiveBlur) {
    Render::CRenderPass       pass;
    Render::CRenderingContext context{{}, pass};
    CTexPassElement           element{CTexPassElement::SRenderData{
        .blur             = true,
        .liveBlurOverride = true,
    }};

    EXPECT_TRUE(element.needsLiveBlur(context));
    EXPECT_FALSE(element.needsPrecomputeBlur(context));
}

TEST(TexPassElement, LiveBlurOverrideForcesPrecomputedBlur) {
    Render::CRenderPass       pass;
    Render::CRenderingContext context{{}, pass};
    CTexPassElement           element{CTexPassElement::SRenderData{
        .blur             = true,
        .liveBlurOverride = false,
    }};

    EXPECT_FALSE(element.needsLiveBlur(context));
    EXPECT_TRUE(element.needsPrecomputeBlur(context));
}
