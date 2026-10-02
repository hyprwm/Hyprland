#include <render/Context.hpp>
#include <render/Renderer.hpp>
#include <render/ElementRenderer.hpp>
#include <render/pass/BackdropScopePassElement.hpp>
#include <protocols/types/Buffer.hpp>

#include <gtest/gtest.h>
#include <type_traits>

namespace Render {
    class CContextMetadataElement : public IPassElement {
      public:
        bool needsLiveBlur(CRenderContext& ctx) override {
            return ctx.m_renderingSnapshot;
        }
        bool needsPrecomputeBlur(CRenderContext& ctx) override {
            return !ctx.m_renderingSnapshot;
        }
        const char* passName() override {
            return "CContextMetadataElement";
        }
        ePassElementType type() override {
            return EK_CUSTOM;
        }
    };

    class CContextChildrenElement : public CContextMetadataElement {
      public:
        explicit CContextChildrenElement(bool parent) : m_parent(parent) {
            ;
        }
        std::vector<UP<IPassElement>> draw(CRenderContext& ctx) override {
            ++ctx.m_data.mouseZoomFactor;
            std::vector<UP<IPassElement>> children;
            if (m_parent)
                children.emplace_back(makeUnique<CContextChildrenElement>(false));
            else
                children.emplace_back(makeUnique<CBorderPassElement>(CBorderPassElement::SBorderData{}));
            return children;
        }

      private:
        bool m_parent = false;
    };

    class CContextElementRenderer : public IElementRenderer {
      public:
        size_t m_draws = 0;

      private:
        void draw(CRenderContext& ctx, WP<CBorderPassElement>, const CRegion& damage) override {
            ++m_draws;
            EXPECT_EQ(ctx.m_data.mouseZoomFactor, 5.F);
            EXPECT_EQ(damage.copy().getExtents(), CBox(1, 2, 3, 4));
            ctx.m_data.clipBox = damage.copy().getExtents();
        }
        void draw(CRenderContext&, WP<CClearPassElement>, const CRegion&) override {
            ADD_FAILURE();
        }
        void draw(CRenderContext&, WP<CFramebufferElement>, const CRegion&) override {
            ADD_FAILURE();
        }
        void draw(CRenderContext&, WP<CPreBlurElement>, const CRegion&) override {
            ADD_FAILURE();
        }
        void draw(CRenderContext&, WP<CRectPassElement>, const CRegion&) override {
            ADD_FAILURE();
        }
        void draw(CRenderContext&, WP<CShadowPassElement>, const CRegion&) override {
            ADD_FAILURE();
        }
        void draw(CRenderContext&, WP<CInnerGlowPassElement>, const CRegion&) override {
            ADD_FAILURE();
        }
        void draw(CRenderContext&, WP<CTexPassElement>, const CRegion&) override {
            ADD_FAILURE();
        }
        void draw(CRenderContext&, WP<CTextureMatteElement>, const CRegion&) override {
            ADD_FAILURE();
        }
    };

    TEST(RenderContext, NestedPassMetadataUsesSuppliedContext) {
        CRenderContext first, second;
        first.m_renderingSnapshot = true;
        auto nested               = makeUnique<CRenderPass>();
        auto metadata             = makeUnique<CContextMetadataElement>();
        nested->add(std::move(metadata));
        CTransformedWindowPassElement element{CTransformedWindowPassElement::SData{.pass = std::move(nested)}};

        EXPECT_TRUE(element.needsLiveBlur(first));
        EXPECT_FALSE(element.needsPrecomputeBlur(first));
        EXPECT_FALSE(element.needsLiveBlur(second));
        EXPECT_TRUE(element.needsPrecomputeBlur(second));
        EXPECT_TRUE(element.needsLiveBlur(first));
    }

    TEST(RenderContext, RoutingIsPerContextAndRestoresNestedDestinations) {
        CRenderContext first, second;
        CRenderPass    outer, inner;
        auto           outerGuard = IHyprRenderer::redirectPass(first, &outer);
        EXPECT_EQ(&IHyprRenderer::currentPass(first), &outer);
        EXPECT_EQ(&IHyprRenderer::currentPass(second), &second.m_pass);

        IHyprRenderer::addPassElement(first, makeUnique<CContextMetadataElement>());
        IHyprRenderer::addPassElement(second, makeUnique<CContextMetadataElement>());
        {
            auto innerGuard = IHyprRenderer::redirectPass(first, &inner);
            IHyprRenderer::addPassElement(first, makeUnique<CContextMetadataElement>());
            EXPECT_EQ(&IHyprRenderer::currentPass(first), &inner);
            EXPECT_EQ(&IHyprRenderer::currentPass(second), &second.m_pass);
        }
        EXPECT_EQ(&IHyprRenderer::currentPass(first), &outer);
        EXPECT_TRUE(outer.single());
        EXPECT_TRUE(inner.single());
        EXPECT_TRUE(second.m_pass.single());
        EXPECT_FALSE(first.m_pass.single());
        outerGuard.reset();
        EXPECT_EQ(&IHyprRenderer::currentPass(first), &first.m_pass);
    }

    TEST(RenderContext, CustomChildrenAndBackendReceiveSuppliedContext) {
        CRenderContext first, second;
        second.m_data.mouseZoomFactor = 3.F;
        CContextElementRenderer renderer;
        auto                    parent = makeUnique<CContextChildrenElement>(true);
        renderer.drawElement(second, parent, CRegion{1, 2, 3, 4});

        EXPECT_EQ(renderer.m_draws, 1U);
        EXPECT_EQ(second.m_data.mouseZoomFactor, 5.F);
        EXPECT_EQ(second.m_data.clipBox, CBox(1, 2, 3, 4));
        EXPECT_EQ(first.m_data.mouseZoomFactor, 1.F);
        EXPECT_TRUE(first.m_data.clipBox.empty());
    }

    // Ownership tests only: any actual framebuffer operation is unexpected.
    class CContextTestFramebuffer : public IFramebuffer {
      public:
        void release() override {
            ADD_FAILURE() << "Unexpected framebuffer release";
        }
        bool readPixels(CHLBufferReference, uint32_t, uint32_t, uint32_t, uint32_t) override {
            ADD_FAILURE() << "Unexpected framebuffer readback";
            return false;
        }
        void bind() override {
            ADD_FAILURE() << "Unexpected framebuffer bind";
        }
        void addStencil(SP<ITexture>) override {
            ADD_FAILURE() << "Unexpected stencil attachment";
        }

      private:
        bool internalAlloc(int, int, DRMFormat) override {
            ADD_FAILURE() << "Unexpected framebuffer allocation";
            return false;
        }
    };

    static_assert(!std::is_copy_constructible_v<CRenderContext>);
    static_assert(!std::is_move_constructible_v<CRenderContext>);
    static_assert(std::is_copy_constructible_v<SRenderData>);

    TEST(RenderContext, ResetReleasesTargetsAndPassContents) {
        CRenderContext context;
        ASSERT_TRUE(context.begin());

        auto             fb      = makeShared<CContextTestFramebuffer>();
        WP<IFramebuffer> target  = fb;
        context.m_data.currentFB = fb;
        context.m_data.mainFB    = fb;
        context.m_data.outFB     = fb;
        context.m_backdropCaptures.push_back({.framebuffer = fb});
        fb.reset();

        auto               scope       = makeShared<SBackdropScope>();
        WP<SBackdropScope> passContent = scope;
        context.m_pass.add(makeUnique<CBackdropScopePassElement>(CBackdropScopePassElement::eAction::BEGIN, scope));
        scope.reset();
        ASSERT_FALSE(target.expired());
        ASSERT_FALSE(passContent.expired());

        context.reset();
        EXPECT_FALSE(context.active());
        EXPECT_TRUE(target.expired());
        EXPECT_TRUE(passContent.expired());
        EXPECT_FALSE(context.m_data.currentFB);
        EXPECT_FALSE(context.m_data.mainFB);
        EXPECT_FALSE(context.m_data.outFB);
    }

    TEST(RenderContext, RejectedBeginLeavesActiveSessionIntact) {
        CRenderContext context;
        CRenderPass    nestedPass;
        ASSERT_TRUE(context.begin());
        context.m_mode                 = RENDER_MODE_TO_BUFFER;
        context.m_currentPass          = &nestedPass;
        context.m_data.mouseZoomFactor = 2.5F;
        context.m_data.damage          = CRegion{10, 20, 30, 40};
        auto fb                        = makeShared<CContextTestFramebuffer>();
        context.m_data.currentFB       = fb;
        auto scope                     = makeShared<SBackdropScope>();
        context.m_pass.add(makeUnique<CBackdropScopePassElement>(CBackdropScopePassElement::eAction::BEGIN, scope));

        EXPECT_FALSE(context.begin());
        EXPECT_TRUE(context.active());
        EXPECT_EQ(context.m_mode, RENDER_MODE_TO_BUFFER);
        EXPECT_EQ(context.m_currentPass, &nestedPass);
        EXPECT_EQ(context.m_data.currentFB, fb);
        EXPECT_FLOAT_EQ(context.m_data.mouseZoomFactor, 2.5F);
        EXPECT_EQ(context.m_data.damage.getExtents(), CBox(10, 20, 30, 40));
        EXPECT_TRUE(context.m_pass.single());
    }

    TEST(RenderContext, ReuseResetsStateAndRetainsScratchCapacity) {
        CRenderContext context;
        context.m_cmSettingsCache.reserve(16);
        context.m_backdropCaptures.reserve(8);
        context.m_data.renderModif.modifs.reserve(32);
        const auto cmCapacity       = context.m_cmSettingsCache.capacity();
        const auto backdropCapacity = context.m_backdropCaptures.capacity();
        const auto modifCapacity    = context.m_data.renderModif.modifs.capacity();

        for (const auto mode : {RENDER_MODE_FULL_FAKE, RENDER_MODE_TO_BUFFER, RENDER_MODE_TO_BUFFER_READ_ONLY, RENDER_MODE_NORMAL}) {
            ASSERT_TRUE(context.begin());
            context.m_mode                             = mode;
            context.m_currentPass                      = &context.m_pass;
            context.m_data.mouseZoomFactor             = 3.F;
            context.m_data.mouseZoomUseMouse           = false;
            context.m_data.useNearestNeighbor          = true;
            context.m_data.blockScreenShader           = true;
            context.m_data.transformDamage             = false;
            context.m_data.noSimplify                  = true;
            context.m_data.renderingTransformedSource  = true;
            context.m_data.primarySurfaceUVTopLeft     = {0, 0};
            context.m_data.primarySurfaceUVBottomRight = {1, 1};
            context.m_data.clipBox                     = {1, 2, 3, 4};
            context.m_data.fbSize                      = {100, 200};
            context.m_data.damage                      = CRegion{0, 0, 100, 200};
            context.m_data.finalDamage                 = context.m_data.damage;
            context.m_data.renderModif.enabled         = false;
            context.m_data.renderModif.modifs.emplace_back(SRenderModifData::RMOD_TYPE_SCALE, 2.F);
            context.m_cmSettingsCache.emplace_back();
            context.m_backdropCaptures.emplace_back();
            context.m_blockSurfaceFeedback = true;
            context.m_renderingSnapshot    = true;
            context.m_swapchainAcquired    = true;
            context.m_gl                   = {.fakeFrame = true, .offloadedFramebuffer = true, .applyFinalShader = true};

            context.reset();
            ASSERT_TRUE(context.begin());
            EXPECT_EQ(context.m_mode, RENDER_MODE_NORMAL);
            EXPECT_EQ(context.m_currentPass, nullptr);
            EXPECT_FLOAT_EQ(context.m_data.mouseZoomFactor, 1.F);
            EXPECT_TRUE(context.m_data.mouseZoomUseMouse);
            EXPECT_FALSE(context.m_data.useNearestNeighbor);
            EXPECT_FALSE(context.m_data.blockScreenShader);
            EXPECT_TRUE(context.m_data.transformDamage);
            EXPECT_FALSE(context.m_data.noSimplify);
            EXPECT_FALSE(context.m_data.renderingTransformedSource);
            EXPECT_EQ(context.m_data.primarySurfaceUVTopLeft, Vector2D(-1, -1));
            EXPECT_EQ(context.m_data.primarySurfaceUVBottomRight, Vector2D(-1, -1));
            EXPECT_EQ(context.m_data.clipBox, CBox());
            EXPECT_EQ(context.m_data.fbSize, Vector2D(-1, -1));
            EXPECT_TRUE(context.m_data.damage.empty());
            EXPECT_TRUE(context.m_data.finalDamage.empty());
            EXPECT_TRUE(context.m_data.renderModif.enabled);
            EXPECT_TRUE(context.m_data.renderModif.modifs.empty());
            EXPECT_TRUE(context.m_cmSettingsCache.empty());
            EXPECT_TRUE(context.m_backdropCaptures.empty());
            EXPECT_FALSE(context.m_blockSurfaceFeedback);
            EXPECT_FALSE(context.m_renderingSnapshot);
            EXPECT_FALSE(context.m_swapchainAcquired);
            EXPECT_FALSE(context.m_gl.fakeFrame);
            EXPECT_FALSE(context.m_gl.offloadedFramebuffer);
            EXPECT_FALSE(context.m_gl.applyFinalShader);
            EXPECT_EQ(context.m_cmSettingsCache.capacity(), cmCapacity);
            EXPECT_EQ(context.m_backdropCaptures.capacity(), backdropCapacity);
            EXPECT_EQ(context.m_data.renderModif.modifs.capacity(), modifCapacity);
            context.reset();
        }
    }

    TEST(RenderContext, BeginDiscardsInactiveAmbientState) {
        CRenderContext context;
        context.m_data.currentFB      = makeShared<CContextTestFramebuffer>();
        WP<IFramebuffer> staleTarget  = context.m_data.currentFB;
        context.m_renderingSnapshot   = true;
        context.m_gl.applyFinalShader = true;
        ASSERT_TRUE(context.begin());
        EXPECT_TRUE(staleTarget.expired());
        EXPECT_FALSE(context.m_renderingSnapshot);
        EXPECT_FALSE(context.m_gl.applyFinalShader);
    }
}
