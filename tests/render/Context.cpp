#include <render/Context.hpp>
#include <render/Renderer.hpp>
#include <render/ElementRenderer.hpp>
#include <render/pass/BackdropScopePassElement.hpp>
#include <protocols/types/Buffer.hpp>

#include <gtest/gtest.h>
#include <stdexcept>
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

    class CContextTestBuffer : public Aquamarine::IBuffer {
      public:
        Aquamarine::eBufferCapability caps() override {
            return Aquamarine::BUFFER_CAPABILITY_NONE;
        }
        Aquamarine::eBufferType type() override {
            return Aquamarine::BUFFER_TYPE_MISC;
        }
        void update(const CRegion&) override {
            ADD_FAILURE();
        }
        bool isSynchronous() override {
            return true;
        }
        bool good() override {
            return true;
        }
    };

    class CContextTestRenderbuffer : public IRenderbuffer {
      public:
        explicit CContextTestRenderbuffer(SP<Aquamarine::IBuffer> buffer) : IRenderbuffer(buffer, 0) {
            ;
        }
        void bind() override {
            ADD_FAILURE();
        }
        void unbind() override {
            ADD_FAILURE();
        }
    };

    static_assert(!std::is_copy_constructible_v<CRenderContext>);
    static_assert(!std::is_move_constructible_v<CRenderContext>);
    static_assert(std::is_copy_constructible_v<SRenderData>);
    static_assert(!std::is_copy_constructible_v<CRenderDataScope>);
    static_assert(!std::is_move_constructible_v<CRenderDataScope>);
    static_assert(!std::is_copy_constructible_v<CTempFramebufferScope>);
    static_assert(!std::is_move_constructible_v<CTempFramebufferScope>);

    TEST(RenderContext, ScopedDrawRestoresParentStateOnReturnAndException) {
        CRenderContext ctx;
        ctx.m_data.damage              = CRegion{1, 2, 30, 40};
        ctx.m_data.finalDamage         = CRegion{5, 6, 70, 80};
        ctx.m_data.targetProjection    = Mat3x3::identity().translate({12, 34}).rotate(0.5F);
        ctx.m_data.projectionType      = RPT_FB;
        ctx.m_data.fbSize              = {800, 600};
        ctx.m_data.clipBox             = {7, 8, 90, 100};
        ctx.m_data.renderModif.enabled = false;
        ctx.m_data.renderModif.modifs.emplace_back(SRenderModifData::RMOD_TYPE_SCALE, 2.F);
        ctx.m_data.mouseZoomFactor             = 3.F;
        ctx.m_data.mouseZoomUseMouse           = false;
        ctx.m_data.useNearestNeighbor          = true;
        ctx.m_data.blockScreenShader           = true;
        ctx.m_data.primarySurfaceUVTopLeft     = {0.1, 0.2};
        ctx.m_data.primarySurfaceUVBottomRight = {0.8, 0.9};
        ctx.m_data.transformDamage             = false;
        ctx.m_data.noSimplify                  = true;
        ctx.m_data.renderingTransformedSource  = true;
        const auto parent                      = ctx.m_data;

        for (bool fail : {false, true}) {
            const auto nestedDraw = [&] {
                auto state             = ctx.saveDrawState();
                ctx.m_data             = {};
                ctx.m_data.damage      = CRegion{0, 0, 200, 100};
                ctx.m_data.finalDamage = ctx.m_data.damage;
                if (fail)
                    throw std::runtime_error("nested draw failed");
                return;
            };
            if (fail)
                EXPECT_THROW(nestedDraw(), std::runtime_error);
            else
                nestedDraw();

            EXPECT_EQ(ctx.m_data.damage.getExtents(), parent.damage.copy().getExtents());
            EXPECT_EQ(ctx.m_data.finalDamage.getExtents(), parent.finalDamage.copy().getExtents());
            EXPECT_EQ(ctx.m_data.targetProjection, parent.targetProjection);
            EXPECT_EQ(ctx.m_data.projectionType, parent.projectionType);
            EXPECT_EQ(ctx.m_data.fbSize, parent.fbSize);
            EXPECT_EQ(ctx.m_data.clipBox, parent.clipBox);
            EXPECT_EQ(ctx.m_data.renderModif.enabled, parent.renderModif.enabled);
            ASSERT_EQ(ctx.m_data.renderModif.modifs.size(), 1U);
            EXPECT_EQ(ctx.m_data.renderModif.modifs[0].first, SRenderModifData::RMOD_TYPE_SCALE);
            EXPECT_EQ(std::any_cast<float>(ctx.m_data.renderModif.modifs[0].second), 2.F);
            EXPECT_EQ(ctx.m_data.mouseZoomFactor, parent.mouseZoomFactor);
            EXPECT_EQ(ctx.m_data.mouseZoomUseMouse, parent.mouseZoomUseMouse);
            EXPECT_EQ(ctx.m_data.useNearestNeighbor, parent.useNearestNeighbor);
            EXPECT_EQ(ctx.m_data.blockScreenShader, parent.blockScreenShader);
            EXPECT_EQ(ctx.m_data.primarySurfaceUVTopLeft, parent.primarySurfaceUVTopLeft);
            EXPECT_EQ(ctx.m_data.primarySurfaceUVBottomRight, parent.primarySurfaceUVBottomRight);
            EXPECT_EQ(ctx.m_data.transformDamage, parent.transformDamage);
            EXPECT_EQ(ctx.m_data.noSimplify, parent.noSimplify);
            EXPECT_EQ(ctx.m_data.renderingTransformedSource, parent.renderingTransformedSource);
        }
    }

    TEST(RenderContext, NestedDrawRestoresStateBeforeFallback) {
        CRenderContext ctx, other;
        ctx.m_data.damage      = CRegion{1, 2, 3, 4};
        ctx.m_data.finalDamage = CRegion{5, 6, 7, 8};
        auto nestedDraw        = [&] {
            auto state              = ctx.saveDrawState();
            ctx.m_data.damage       = CRegion{10, 20, 30, 40};
            ctx.m_data.finalDamage  = CRegion{50, 60, 70, 80};
            const auto tryOffscreen = [&] {
                auto state = ctx.saveDrawState();
                ctx.m_data = {};
                return false;
            };
            if (tryOffscreen())
                return;

            EXPECT_EQ(ctx.m_data.damage.getExtents(), CBox(10, 20, 30, 40));
            EXPECT_EQ(ctx.m_data.finalDamage.getExtents(), CBox(50, 60, 70, 80));
            auto                    fallback = ctx.saveDrawState();
            CContextElementRenderer renderer;
            auto                    parent = makeUnique<CContextChildrenElement>(true);
            ctx.m_data.mouseZoomFactor     = 3.F;
            renderer.drawElement(ctx, parent, CRegion{1, 2, 3, 4});
            EXPECT_EQ(renderer.m_draws, 1U);
        };
        nestedDraw();
        EXPECT_EQ(ctx.m_data.damage.getExtents(), CBox(1, 2, 3, 4));
        EXPECT_EQ(ctx.m_data.finalDamage.getExtents(), CBox(5, 6, 7, 8));
        EXPECT_TRUE(ctx.m_data.clipBox.empty());
        EXPECT_EQ(ctx.m_data.mouseZoomFactor, 1.F);
        EXPECT_TRUE(other.m_data.damage.empty());
        EXPECT_TRUE(other.m_data.clipBox.empty());
    }

    TEST(RenderContext, ScopedDrawRetainsTargetsAndSessionRouting) {
        CRenderContext ctx;
        ASSERT_TRUE(ctx.begin());
        ctx.m_data.currentFB     = makeShared<CContextTestFramebuffer>();
        ctx.m_data.mainFB        = makeShared<CContextTestFramebuffer>();
        ctx.m_data.outFB         = makeShared<CContextTestFramebuffer>();
        WP<IFramebuffer> current = ctx.m_data.currentFB, main = ctx.m_data.mainFB, out = ctx.m_data.outFB;
        CRenderPass      pass;
        auto             route        = IHyprRenderer::redirectPass(ctx, &pass);
        const auto       buffer       = makeShared<CContextTestBuffer>();
        const auto       renderbuffer = makeShared<CContextTestRenderbuffer>(buffer);
        ctx.m_currentBuffer           = buffer;
        ctx.m_currentRenderbuffer     = renderbuffer;
        ctx.m_mode                    = RENDER_MODE_TO_BUFFER;
        ctx.m_blockSurfaceFeedback    = true;
        ctx.m_renderingSnapshot       = true;
        ctx.m_swapchainAcquired       = true;
        ctx.m_gl                      = {.fakeFrame = true, .offloadedFramebuffer = true, .applyFinalShader = true};
        {
            auto state = ctx.saveDrawState();
            ctx.m_data = {};
            EXPECT_FALSE(current.expired());
            EXPECT_FALSE(main.expired());
            EXPECT_FALSE(out.expired());
            EXPECT_TRUE(ctx.active());
            EXPECT_EQ(ctx.m_currentBuffer, buffer);
            EXPECT_EQ(ctx.m_currentRenderbuffer, renderbuffer);
            EXPECT_EQ(&IHyprRenderer::currentPass(ctx), &pass);
            IHyprRenderer::addPassElement(ctx, makeUnique<CContextMetadataElement>());
        }
        EXPECT_EQ(ctx.m_data.currentFB, current.lock());
        EXPECT_EQ(ctx.m_data.mainFB, main.lock());
        EXPECT_EQ(ctx.m_data.outFB, out.lock());
        EXPECT_EQ(&IHyprRenderer::currentPass(ctx), &pass);
        EXPECT_TRUE(pass.single());
        EXPECT_EQ(ctx.m_currentBuffer, buffer);
        EXPECT_EQ(ctx.m_currentRenderbuffer, renderbuffer);
        EXPECT_TRUE(ctx.active());
        EXPECT_EQ(ctx.m_mode, RENDER_MODE_TO_BUFFER);
        EXPECT_TRUE(ctx.m_blockSurfaceFeedback);
        EXPECT_TRUE(ctx.m_renderingSnapshot);
        EXPECT_TRUE(ctx.m_swapchainAcquired);
        EXPECT_TRUE(ctx.m_gl.fakeFrame);
        EXPECT_TRUE(ctx.m_gl.offloadedFramebuffer);
        EXPECT_TRUE(ctx.m_gl.applyFinalShader);
    }

    TEST(RenderContext, ScopedDrawRestoresInheritedBackdropsWithoutResettingCaches) {
        CRenderContext ctx;
        ctx.m_backdropCaptures.push_back({
            .scope       = makeShared<SBackdropScope>(),
            .framebuffer = makeShared<CContextTestFramebuffer>(),
        });
        WP<SBackdropScope> scope = ctx.m_backdropCaptures.back().scope;
        WP<IFramebuffer>   fb    = ctx.m_backdropCaptures.back().framebuffer;
        ctx.m_backdropCaptures.reserve(8);
        const auto capacity   = ctx.m_backdropCaptures.capacity();
        const auto nestedDraw = [&] {
            auto state = ctx.saveDrawState();
            ASSERT_EQ(ctx.m_backdropCaptures.size(), 1U);
            EXPECT_EQ(ctx.m_backdropCaptures.back().framebuffer, fb.lock());
            ctx.m_backdropCaptures.emplace_back();
            {
                auto inner = ctx.saveDrawState();
                ctx.m_backdropCaptures.emplace_back();
                ctx.m_backdropCaptures.pop_back();
                ctx.m_backdropCaptures.emplace_back();
            }
            EXPECT_EQ(ctx.m_backdropCaptures.size(), 2U);
            EXPECT_FALSE(scope.expired());
            EXPECT_FALSE(fb.expired());
            ctx.m_cmSettingsCache.emplace_back().srcDescId = 42;
            throw std::runtime_error("unclosed nested backdrop");
        };
        EXPECT_THROW(nestedDraw(), std::runtime_error);
        ASSERT_EQ(ctx.m_backdropCaptures.size(), 1U);
        EXPECT_FALSE(scope.expired());
        EXPECT_FALSE(fb.expired());
        EXPECT_EQ(ctx.m_backdropCaptures.back().scope, scope.lock());
        EXPECT_EQ(ctx.m_backdropCaptures.back().framebuffer, fb.lock());
        EXPECT_EQ(ctx.m_backdropCaptures.capacity(), capacity);
        ASSERT_EQ(ctx.m_cmSettingsCache.size(), 1U);
        EXPECT_EQ(ctx.m_cmSettingsCache[0].srcDescId, 42U);
    }

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
