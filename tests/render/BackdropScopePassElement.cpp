#include <render/pass/BackdropScopePassElement.hpp>
#include <render/pass/TexPassElement.hpp>
#include <render/pass/TransformedWindowPassElement.hpp>

#include <gtest/gtest.h>

TEST(BackdropScopePassElement, MarkersAreBalancedPassMetadata) {
    const auto                scope = makeShared<SBackdropScope>();
    CBackdropScopePassElement begin{CBackdropScopePassElement::eAction::BEGIN, scope};
    CBackdropScopePassElement end{CBackdropScopePassElement::eAction::END, scope};

    EXPECT_EQ(begin.action(), CBackdropScopePassElement::eAction::BEGIN);
    EXPECT_EQ(end.action(), CBackdropScopePassElement::eAction::END);
    EXPECT_EQ(begin.scope(), scope);
    EXPECT_EQ(end.scope(), scope);
    EXPECT_EQ(begin.type(), EK_BACKDROP_SCOPE);
    EXPECT_EQ(end.type(), EK_BACKDROP_SCOPE);
}

TEST(BackdropScopePassElement, MarkersSurviveSimplificationWithoutRequestingBlur) {
    const auto                scope = makeShared<SBackdropScope>();
    CBackdropScopePassElement marker{CBackdropScopePassElement::eAction::BEGIN, scope};

    EXPECT_TRUE(marker.undiscardable());
    EXPECT_FALSE(marker.needsLiveBlur());
    EXPECT_FALSE(marker.needsPrecomputeBlur());
    EXPECT_FALSE(marker.disableSimplification());
    EXPECT_FALSE(marker.boundingBox().has_value());
    EXPECT_TRUE(marker.opaqueRegion().empty());
}

TEST(BackdropScopePlanner, ActivatesOnlyInnermostScopeAndClipsDamage) {
    CBackdropScopePlanner planner;
    const auto            outer = makeShared<SBackdropScope>();
    const auto            inner = makeShared<SBackdropScope>();

    planner.begin(outer);
    planner.begin(inner);
    planner.addLiveBlur(CRegion{80, 80, 40, 40});
    planner.end(inner, CBox{0, 0, 100, 100});
    planner.end(outer, CBox{0, 0, 100, 100});

    EXPECT_FALSE(outer->required);
    EXPECT_TRUE(outer->damage.empty());
    EXPECT_TRUE(inner->required);
    EXPECT_EQ(inner->damage.getExtents(), CBox(80, 80, 20, 20));
    EXPECT_TRUE(planner.empty());
}

TEST(BackdropScopePlanner, UnionsLiveBlurDamageWithinScope) {
    CBackdropScopePlanner planner;
    const auto            scope = makeShared<SBackdropScope>();

    planner.begin(scope);
    planner.addLiveBlur(CRegion{10, 20, 30, 40});
    planner.addLiveBlur(CRegion{50, 60, 20, 10});
    planner.end(scope, CBox{0, 0, 100, 100});

    EXPECT_TRUE(scope->required);
    EXPECT_EQ(scope->damage.getExtents(), CBox(10, 20, 60, 50));
}

TEST(BackdropScopePlanner, TransformedWindowReportsNestedLiveBlur) {
    auto nestedPass = makeUnique<Render::CRenderPass>();
    nestedPass->add(makeUnique<CTexPassElement>(CTexPassElement::SRenderData{
        .blur                  = true,
        .blockBlurOptimization = true,
    }));

    CTransformedWindowPassElement transformed{CTransformedWindowPassElement::SData{.pass = std::move(nestedPass)}};
    EXPECT_TRUE(transformed.needsLiveBlur());
}
