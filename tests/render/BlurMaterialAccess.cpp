#include <render/Context.hpp>
#include <render/Framebuffer.hpp>
#include <render/SceneResources.hpp>
#include <render/gl/blur/Material.hpp>
#include <protocols/types/Buffer.hpp>

#include <gtest/gtest.h>

namespace Render::GL {
    class CAccessTestMaterial : public IGLBlurMaterial {
      public:
        eBlurType type() const noexcept override {
            return eBlurType::BLUR_DUAL_KAWASE;
        }
        SBlurMaterialRequirements requirements() const noexcept override {
            return {};
        }
        void prepare(CRenderContext& ctx, const SBlurMaterialContext& context) override {
            EXPECT_FALSE(ctx.readOnlyEffects());
            ++m_prepareCalls;
            m_strength = context.strength;
        }

        int   m_prepareCalls = 0;
        float m_strength     = 0.F;
    };

    class CAccessTestTexture : public ITexture {
      public:
        bool ok() override {
            return m_valid;
        }
        void setTexParameter(GLenum, GLint) override {
            ADD_FAILURE() << "Texture lookup must not change sampling parameters";
        }
        void allocate(const Vector2D&, uint32_t) override {
            ADD_FAILURE() << "Texture lookup must not allocate";
        }
        void update(uint32_t, uint8_t*, uint32_t, const CRegion&) override {
            ADD_FAILURE() << "Texture lookup must not update contents";
        }
        void bind() override {
            ADD_FAILURE() << "Texture lookup must not bind";
        }

        bool m_valid = true;
    };

    class CAccessTestFramebuffer : public IFramebuffer {
      public:
        explicit CAccessTestFramebuffer(SP<ITexture> texture) : m_attachment(texture) {
            m_tex = texture;
        }
        void release() override {
            m_fbAllocated = false;
            m_tex.reset();
            m_size = {};
        }
        bool readPixels(CHLBufferReference, uint32_t, uint32_t, uint32_t, uint32_t) override {
            ADD_FAILURE() << "Texture lookup must not read back";
            return false;
        }
        void bind() override {
            ADD_FAILURE() << "Texture lookup must not bind";
        }
        void addStencil(SP<ITexture>) override {
            ADD_FAILURE() << "Texture lookup must not change attachments";
        }

      private:
        bool internalAlloc(int, int, DRMFormat) override {
            m_tex = m_attachment;
            return true;
        }

        SP<ITexture> m_attachment;
    };

    TEST(BlurMaterialAccess, MonitorPreparationStillDispatchesVirtualPrepare) {
        CAccessTestMaterial        material;
        CRenderContext             ctx;
        const SBlurContext         blurContext;
        const CRegion              damage;
        const SBlurMaterialContext context{
            .blurContext  = blurContext,
            .outputDamage = damage,
            .strength     = 0.5F,
        };

        ASSERT_TRUE(ctx.begin());
        material.prepareForFrame(ctx, context);
        EXPECT_EQ(material.m_prepareCalls, 1);
        EXPECT_FLOAT_EQ(material.m_strength, context.strength);

        ctx.reset();
        ASSERT_TRUE(ctx.begin(makeShared<CSceneResources>(PHLMONITORREF{})));
        material.prepareForFrame(ctx, context);
        EXPECT_EQ(material.m_prepareCalls, 2);
    }

    TEST(BlurMaterialAccess, IsolatedPreparationStaysReadOnlyAcrossNestedDrawsAndRejectedBegin) {
        CAccessTestMaterial        material;
        CRenderContext             ctx;
        const auto                 resources = makeShared<CSceneResources>(SP<IFramebuffer>{});
        const SBlurContext         blurContext;
        const CRegion              damage;
        const SBlurMaterialContext context{
            .blurContext  = blurContext,
            .outputDamage = damage,
        };

        ASSERT_TRUE(ctx.begin(resources));
        const auto frameTime = ctx.effectTime();
        material.prepareForFrame(ctx, context);
        {
            auto outer        = ctx.saveDrawState();
            ctx.m_data        = {};
            ctx.m_data.fbSize = {100, 200};
            material.prepareForFrame(ctx, context);
            {
                auto inner = ctx.saveDrawState();
                ctx.m_data = {};
                EXPECT_FALSE(ctx.begin());
                EXPECT_FALSE(ctx.begin(makeShared<CSceneResources>(PHLMONITORREF{})));
                material.prepareForFrame(ctx, context);
                EXPECT_EQ(material.m_prepareCalls, 0);
                EXPECT_EQ(ctx.effectTime(), frameTime);
                EXPECT_EQ(ctx.sceneResources(), resources);
            }
            EXPECT_EQ(ctx.m_data.fbSize, Vector2D(100, 200));
            material.prepareForFrame(ctx, context);
            EXPECT_EQ(ctx.effectTime(), frameTime);
        }
        material.prepareForFrame(ctx, context);
        EXPECT_EQ(material.m_prepareCalls, 0);
        EXPECT_TRUE(ctx.readOnlyEffects());
        EXPECT_EQ(ctx.effectTime(), frameTime);

        ctx.reset();
        ASSERT_TRUE(ctx.begin());
        material.prepareForFrame(ctx, context);
        EXPECT_EQ(material.m_prepareCalls, 1);
    }

    TEST(BlurMaterialAccess, MissingUnallocatedAndReleasedFramebuffersHaveNoMaterialTexture) {
        EXPECT_FALSE(materialTexture(nullptr));

        const auto texture     = makeShared<CAccessTestTexture>();
        const auto framebuffer = makeShared<CAccessTestFramebuffer>(texture);
        EXPECT_EQ(framebuffer->getTexture(), texture);
        EXPECT_FALSE(materialTexture(framebuffer));

        ASSERT_TRUE(framebuffer->alloc(32, 32));
        EXPECT_EQ(materialTexture(framebuffer), texture);
        framebuffer->release();
        EXPECT_FALSE(materialTexture(framebuffer));
        EXPECT_TRUE(texture->ok());
    }

    TEST(BlurMaterialAccess, MissingAndInvalidTexturesAreOmittedWithoutChangingState) {
        const auto missing = makeShared<CAccessTestFramebuffer>(nullptr);
        ASSERT_TRUE(missing->alloc(32, 32));
        EXPECT_FALSE(materialTexture(missing));
        EXPECT_FALSE(missing->getTexture());

        const auto texture     = makeShared<CAccessTestTexture>();
        const auto framebuffer = makeShared<CAccessTestFramebuffer>(texture);
        ASSERT_TRUE(framebuffer->alloc(32, 32));
        texture->m_valid = false;
        EXPECT_FALSE(materialTexture(framebuffer));
        EXPECT_TRUE(framebuffer->isAllocated());
        EXPECT_EQ(framebuffer->getTexture(), texture);
        EXPECT_FALSE(texture->m_valid);

        texture->m_valid = true;
        EXPECT_EQ(materialTexture(framebuffer), texture);
        EXPECT_EQ(materialTexture(framebuffer), texture);
        EXPECT_EQ(framebuffer->m_size, Vector2D(32, 32));
    }
}
