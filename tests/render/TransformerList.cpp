#include <render/transformer/TransformerList.hpp>
#include <render/Context.hpp>
#include <render/WindowRenderPresentation.hpp>

#include <gtest/gtest.h>

class CTestDamageTransformer : public Render::IWindowTransformer {
  public:
    CTestDamageTransformer(int priority, double scale, double add, bool active = true) : m_priority(priority), m_scale(scale), m_add(add), m_active(active) {
        ;
    }

    virtual Render::SWindowTransformBuffer transform(Render::CRenderContext& ctx, const Render::SWindowTransformBuffer& in, const Render::SWindowTransformContext& context) {
        (void)context;
        return in;
    }

    virtual int priority() const {
        return m_priority;
    }

    virtual bool active() const {
        return m_active;
    }

    virtual CBox transformBoxForDamage(const CBox& currentBox) const {
        CBox box = currentBox;
        box.x    = box.x * m_scale + m_add;
        box.w *= m_scale;
        return box;
    }

  private:
    int    m_priority = 0;
    double m_scale    = 1.0;
    double m_add      = 0.0;
    bool   m_active   = true;
};

class CTestRegionTransformer : public Render::IWindowTransformer {
  public:
    CTestRegionTransformer(int priority, double translation) : m_priority(priority), m_translation(translation) {
        ;
    }

    virtual Render::SWindowTransformBuffer transform(Render::CRenderContext& ctx, const Render::SWindowTransformBuffer& in, const Render::SWindowTransformContext& context) {
        (void)context;
        return in;
    }

    virtual int priority() const {
        return m_priority;
    }

    virtual CBox transformedExtents(const CBox& currentBox) const {
        return currentBox.copy().translate({m_translation, 0.0});
    }

    virtual CBox sourceBoxForOutput(const CBox& outputBox, const CBox& inputBox) const {
        return outputBox.copy().translate({-m_translation, 0.0}).intersection(inputBox);
    }

  private:
    int    m_priority    = 0;
    double m_translation = 0.0;
};

class CTestPresentationTransformer : public CTestDamageTransformer {
  public:
    CTestPresentationTransformer(bool active) : CTestDamageTransformer(0, 1.0, 0.0, active) {
        ;
    }

    virtual void amendTransformedRenderData(Render::CRenderContext& ctx, const CBox& currentBox, SMotionBlurData* motionBlurData,
                                            const Render::SWindowRenderPresentation& presentation) {
        ++m_amendCount;
        m_presentation = presentation;
        if (motionBlurData)
            motionBlurData->current = currentBox;
    }

    int                               m_amendCount = 0;
    Render::SWindowRenderPresentation m_presentation;
};

TEST(Render, transformerListDamageBoxFollowsPriorityOrder) {
    Render::CWindowTransformerList list;
    list.emplace<CTestDamageTransformer>(20, 2.0, 0.0);
    list.emplace<CTestDamageTransformer>(10, 1.0, 3.0);

    const CBox BOX = {1, 2, 10, 20};
    const auto OUT = list.transformBoxForDamage(BOX);

    EXPECT_DOUBLE_EQ(OUT.x, 8.0);
    EXPECT_DOUBLE_EQ(OUT.y, 2.0);
    EXPECT_DOUBLE_EQ(OUT.w, 20.0);
    EXPECT_DOUBLE_EQ(OUT.h, 20.0);
}

TEST(Render, transformerListDamageBoxSkipsInactiveTransformers) {
    Render::CWindowTransformerList list;
    list.emplace<CTestDamageTransformer>(10, 2.0, 0.0, false);
    list.emplace<CTestDamageTransformer>(20, 1.0, 3.0);

    const CBox BOX = {1, 2, 10, 20};
    const auto OUT = list.transformBoxForDamage(BOX);

    EXPECT_DOUBLE_EQ(OUT.x, 4.0);
    EXPECT_DOUBLE_EQ(OUT.w, 10.0);
}

TEST(Render, transformerListPlansRequiredSourceInReverseOrder) {
    Render::CWindowTransformerList list;
    list.emplace<CTestRegionTransformer>(20, 5.0);
    list.emplace<CTestRegionTransformer>(10, 10.0);

    const auto PLAN = list.plan({0, 0, 100, 50}, {30, 0, 20, 50});

    ASSERT_EQ(PLAN.stages.size(), 2u);
    EXPECT_EQ(PLAN.sourceBox, CBox(15, 0, 20, 50));
    EXPECT_EQ(PLAN.stages[0].outputBox, CBox(25, 0, 20, 50));
    EXPECT_EQ(PLAN.stages[1].outputBox, CBox(30, 0, 20, 50));
}

TEST(Render, transformerPixelBoxRoundsOutward) {
    EXPECT_EQ(Render::pixelBoxForLogical({-1.25, 2.25, 3.5, 4.5}, 2.0), CBox(-3, 4, 8, 10));
}

TEST(Render, transformerListAmendsWithExplicitPresentationPerRender) {
    Render::CRenderContext                  ctx;
    Render::CWindowTransformerList          list;
    const auto                              ACTIVE   = list.emplace<CTestPresentationTransformer>(true);
    const auto                              INACTIVE = list.emplace<CTestPresentationTransformer>(false);
    const Render::SWindowRenderPresentation FIRST    = {
        .workspaceOffset          = {100, -20},
        .floatingOffset           = {5, 10},
        .workspaceAlpha           = 0.5F,
        .alpha                    = 0.25F,
        .fadeAlpha                = 0.75F,
        .workspaceOffsetAnimating = true,
        .alphaVisible             = false,
    };
    const Render::SWindowRenderPresentation SECOND = {
        .workspaceOffset = {-50, 40},
        .floatingOffset  = {-5, 20},
        .workspaceAlpha  = 0.8F,
        .alpha           = 0.6F,
        .fadeAlpha       = 0.9F,
    };
    const CBox      BOX = {1, 2, 10, 20};
    SMotionBlurData motionBlur;

    list.amendTransformedRenderData(ctx, BOX, &motionBlur, FIRST);
    EXPECT_EQ(ACTIVE->m_presentation, FIRST);
    EXPECT_EQ(ACTIVE->m_amendCount, 1);
    EXPECT_EQ(motionBlur.current, BOX);

    list.amendTransformedRenderData(ctx, BOX, &motionBlur, {});
    EXPECT_EQ(ACTIVE->m_presentation, Render::SWindowRenderPresentation{});
    EXPECT_EQ(ACTIVE->m_amendCount, 2);

    list.amendTransformedRenderData(ctx, BOX, &motionBlur, SECOND);
    EXPECT_EQ(ACTIVE->m_presentation, SECOND);
    EXPECT_EQ(ACTIVE->m_amendCount, 3);
    EXPECT_EQ(INACTIVE->m_amendCount, 0);
}

TEST(Render, transformerListPresentationCopyIsIndependent) {
    Render::CRenderContext            ctx;
    Render::CWindowTransformerList    list;
    const auto                        ACTIVE       = list.emplace<CTestPresentationTransformer>(true);
    const CBox                        BOX          = {1, 2, 10, 20};
    Render::SWindowRenderPresentation presentation = {
        .workspaceOffset          = {100, -20},
        .floatingOffset           = {5, 10},
        .workspaceAlpha           = 0.5F,
        .alpha                    = 0.25F,
        .fadeAlpha                = 0.75F,
        .workspaceOffsetAnimating = true,
        .alphaVisible             = false,
    };
    const auto ORIGINAL = presentation;

    list.amendTransformedRenderData(ctx, BOX, nullptr, presentation);
    presentation = {};
    EXPECT_EQ(ACTIVE->m_presentation, ORIGINAL);

    list.amendTransformedRenderData(ctx, BOX, nullptr, presentation);
    EXPECT_EQ(ACTIVE->m_presentation, presentation);
    EXPECT_NE(ACTIVE->m_presentation, ORIGINAL);
    EXPECT_EQ(ACTIVE->m_amendCount, 2);
}

TEST(Render, transformerListPresentationIgnoresResolvedPresence) {
    Render::CRenderContext           ctx;
    Render::CWindowTransformerList   list;
    const auto                       ACTIVE = list.emplace<CTestPresentationTransformer>(true);
    const CBox                       BOX    = {1, 2, 10, 20};
    Render::SWindowPresentationState state;
    const auto                       ABSENT = Render::resolveWindowPresentation(state);
    state.hasWorkspacePresentation          = true;
    const auto PRESENT                      = Render::resolveWindowPresentation(state);

    list.amendTransformedRenderData(ctx, BOX, nullptr, ABSENT);
    const auto ABSENTCOPY = ACTIVE->m_presentation;
    list.amendTransformedRenderData(ctx, BOX, nullptr, PRESENT);

    EXPECT_EQ(ACTIVE->m_presentation, ABSENTCOPY);
    EXPECT_EQ(ACTIVE->m_amendCount, 2);
}
