#include <render/pass/TexPassElement.hpp>

#include <gtest/gtest.h>

TEST(TexPassElement, ReportsNoBlur) {
    CTexPassElement element{CTexPassElement::SRenderData{}};

    EXPECT_FALSE(element.needsLiveBlur());
    EXPECT_FALSE(element.needsPrecomputeBlur());
}

TEST(TexPassElement, ReportsExplicitLiveBlur) {
    CTexPassElement element{CTexPassElement::SRenderData{
        .blur                  = true,
        .blockBlurOptimization = true,
    }};

    EXPECT_TRUE(element.needsLiveBlur());
    EXPECT_FALSE(element.needsPrecomputeBlur());
}

TEST(TexPassElement, LiveBlurOverrideForcesLiveBlur) {
    CTexPassElement element{CTexPassElement::SRenderData{
        .blur             = true,
        .liveBlurOverride = true,
    }};

    EXPECT_TRUE(element.needsLiveBlur());
    EXPECT_FALSE(element.needsPrecomputeBlur());
}

TEST(TexPassElement, LiveBlurOverrideForcesPrecomputedBlur) {
    CTexPassElement element{CTexPassElement::SRenderData{
        .blur             = true,
        .liveBlurOverride = false,
    }};

    EXPECT_FALSE(element.needsLiveBlur());
    EXPECT_TRUE(element.needsPrecomputeBlur());
}
