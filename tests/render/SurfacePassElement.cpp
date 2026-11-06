#include <render/pass/SurfacePassElement.hpp>
#include <render/Context.hpp>

#include <gtest/gtest.h>

TEST(SurfacePassElement, EmptySurfaceDoesNotRequireMonitorForOpaqueRegion) {
    Render::CRenderContext ctx;
    CSurfacePassElement    element{CSurfacePassElement::SRenderData{
        .rounding = Math::SCornerRadii{0, 12, 24, 6},
    }};

    EXPECT_TRUE(element.opaqueRegion(ctx).empty());
}
