#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <desktop/view/WLSurface.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <output/Monitor.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/core/Compositor.hpp>
#include <render/Context.hpp>
#include <render/Renderer.hpp>
#include <render/pass/SurfacePassElement.hpp>
#include <render/transformer/Transformer.hpp>

#include <gtest/gtest.h>
#include <array>
#include <memory>
#include <sys/socket.h>

using namespace Hyprutils::OS;

class CSurfacePassTestOutput : public Aquamarine::IOutput {
  public:
    bool commit() override {
        ADD_FAILURE() << "Unexpected output commit";
        return false;
    }
    bool test() override {
        ADD_FAILURE() << "Unexpected output test";
        return false;
    }
    SP<Aquamarine::IBackendImplementation> getBackend() override {
        return nullptr;
    }
    std::vector<SDRMFormat> getRenderFormats() override {
        return {};
    }
    bool pendingPageFlip() override {
        return false;
    }
    bool pendingIdleFrame() override {
        return false;
    }
};

class CSurfacePassTestTexture : public Render::ITexture {
  public:
    void setTexParameter(GLenum, GLint) override {
        ADD_FAILURE();
    }
    void allocate(const Vector2D&, uint32_t) override {
        ADD_FAILURE();
    }
    void update(uint32_t, uint8_t*, uint32_t, const CRegion&) override {
        ADD_FAILURE();
    }
};

// Real surface and monitor metadata, without a window, renderer or connected output.
class CSurfacePassElementTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        std::array<int, 2> sockets = {-1, -1};
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        CFileDescriptor server{sockets[0]};
        m_socket    = CFileDescriptor{sockets[1]};
        auto client = wl_client_create(m_display.get(), server.get());
        ASSERT_NE(client, nullptr);
        server.take();

        m_previousCompositor   = std::move(g_pCompositor);
        m_previousEventLoop    = std::move(g_pEventLoopManager);
        m_previousRenderer     = std::move(g_pHyprRenderer);
        m_previousEventBus     = std::move(Event::bus());
        m_previousPresentation = std::move(PROTO::presentation);
        m_previousConfig       = std::move(Config::mgr());
        m_previousWatcher      = std::move(Config::watcher());
        m_previousAnimations   = std::move(Animation::mgr());
        m_previousTree         = std::move(Config::animationTree());
        m_installedGlobals     = true;

        Event::bus()                        = makeUnique<Event::CEventBus>();
        g_pCompositor                       = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay          = m_display.get();
        g_pCompositor->m_explicitConfigPath = "surface-pass-element-test.lua";
        Config::Supplementary::Jeremy::flushCachedCfgPath();
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
        Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree() = makeUnique<Config::CAnimationTreeController>();
        PROTO::presentation     = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "surface-pass-element-test");

        m_monitor                 = makeShared<Monitor::CMonitor>(makeShared<CSurfacePassTestOutput>());
        m_monitor->m_self         = m_monitor;
        m_monitor->m_position     = {};
        m_surface                 = makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(client, 6, 2));
        m_surface->m_self         = m_surface;
        m_surface->m_current.size = m_surface->m_current.bufferSize = {100, 80};
        m_surface->m_current.opaque                                 = CBox{0, 0, 100, 80};
        m_hlSurface                                                 = Desktop::View::CWLSurface::create();
        m_hlSurface->assign(m_surface);
        m_texture           = makeShared<CSurfacePassTestTexture>();
        m_texture->m_opaque = true;
    }

    void TearDown() override {
        m_hlSurface.reset();
        m_surface.reset();
        m_monitor.reset();
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        if (!m_installedGlobals)
            return;

        PROTO::presentation     = std::move(m_previousPresentation);
        Animation::mgr()        = std::move(m_previousAnimations);
        Config::animationTree() = std::move(m_previousTree);
        Config::mgr()           = std::move(m_previousConfig);
        Config::watcher()       = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pCompositor       = std::move(m_previousCompositor);
        g_pEventLoopManager = std::move(m_previousEventLoop);
        g_pHyprRenderer     = std::move(m_previousRenderer);
        Event::bus()        = std::move(m_previousEventBus);
        Config::Supplementary::Jeremy::flushCachedCfgPath();
    }

    CSurfacePassElement::SRenderData data() {
        return {
            .pMonitor        = m_monitor,
            .pos             = m_monitor->m_position + Vector2D{0.8, 0.8},
            .surface         = m_surface,
            .texture         = m_texture,
            .w               = 100,
            .h               = 80,
            .squishOversized = false,
        };
    }

    PHLMONITOR                    m_monitor;
    SP<CWLSurfaceResource>        m_surface;
    SP<Desktop::View::CWLSurface> m_hlSurface;
    SP<CSurfacePassTestTexture>   m_texture;
    Render::CRenderContext        m_ctx;

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    bool                                                       m_installedGlobals = false;
    UP<CCompositor>                                            m_previousCompositor;
    UP<CEventLoopManager>                                      m_previousEventLoop;
    UP<Render::IHyprRenderer>                                  m_previousRenderer;
    UP<Event::CEventBus>                                       m_previousEventBus;
    UP<CPresentationProtocol>                                  m_previousPresentation;
    UP<Config::IConfigManager>                                 m_previousConfig;
    UP<Config::CConfigWatcher>                                 m_previousWatcher;
    UP<Animation::CHyprAnimationManager>                       m_previousAnimations;
    UP<Config::CAnimationTreeController>                       m_previousTree;
};

TEST_F(CSurfacePassElementTest, OpaqueRegionsStayInsideDrawnPixelsAtFractionalScale) {
    for (const auto scale : {1.F, 1.25F, 1.5F, 2.F}) {
        for (const auto origin : {0.0, 1920.0}) {
            for (const bool declaredOpaque : {false, true}) {
                for (const int rounding : {0, 3}) {
                    SCOPED_TRACE(::testing::Message() << scale << ", " << origin << ", " << declaredOpaque << ", " << rounding);
                    m_monitor->m_scale    = scale;
                    m_monitor->m_position = {origin, 0.0};
                    auto renderData       = data();
                    renderData.pos        = m_monitor->m_position + Vector2D{1.0 / scale, 2.0 / scale};
                    renderData.rounding   = rounding;
                    if (!declaredOpaque) {
                        renderData.w = 3836 * (1.0 / scale);
                        renderData.h = 2156 * (1.0 / scale);
                    }
                    CSurfacePassElement element{renderData};
                    const auto          texBox = element.getTexBox();
                    const auto          opaque = element.opaqueRegion(m_ctx);
                    ASSERT_FALSE(opaque.empty());

                    CRegion pixelOpaque;
                    opaque.forEachRect([&](const auto& rect) {
                        EXPECT_GE(rect.x1, texBox.x + rounding);
                        EXPECT_GE(rect.y1, texBox.y + rounding);
                        EXPECT_LE(rect.x2, texBox.x + texBox.w - rounding);
                        EXPECT_LE(rect.y2, texBox.y + texBox.h - rounding);
                        // Match CRenderPass's existing logical-to-pixel opaque conversion.
                        pixelOpaque.add(CBox(rect.x1, rect.y1, rect.x2 - rect.x1, rect.y2 - rect.y1).scale(scale).round());
                    });
                    const CRegion drawn{texBox.copy().scale(scale).round()};
                    EXPECT_TRUE(pixelOpaque.subtract(drawn).empty());
                }
            }
        }
    }
}

TEST_F(CSurfacePassElementTest, DeclaredOpaqueRectsKeepHolesAfterClippingScalingAndInset) {
    m_surface->m_current.opaque = CRegion{-5, -5, 35, 90}.add(CBox{70, -5, 35, 90});
    auto renderData             = data();
    renderData.pos              = {0.55, 0.55};
    renderData.localPos         = {10.25, 5.25};
    renderData.squishOversized  = true;
    renderData.rounding         = 2;
    CSurfacePassElement element{renderData};

    const auto          opaque   = element.opaqueRegion(m_ctx);
    const CRegion       expected = CRegion{13, 8, 22, 70}.add(CBox{76, 8, 22, 70});
    EXPECT_TRUE(pixman_region32_equal(opaque.pixman(), expected.pixman()));
    EXPECT_FALSE(opaque.containsPoint({50, 40}));
}

TEST_F(CSurfacePassElementTest, EveryAlphaChannelPreventsOcclusion) {
    for (const bool declaredOpaque : {false, true}) {
        auto renderData = data();
        if (!declaredOpaque)
            renderData.w = 99.5;
        for (auto channel : {&renderData.alpha, &renderData.fadeAlpha, &m_hlSurface->m_alphaModifier, &m_hlSurface->m_overallOpacity}) {
            *channel = 0.5F;
            CSurfacePassElement translucent{renderData};
            EXPECT_TRUE(translucent.opaqueRegion(m_ctx).empty());
            *channel = 1.F;
            CSurfacePassElement opaque{renderData};
            EXPECT_FALSE(opaque.opaqueRegion(m_ctx).empty());
        }
    }
}

TEST_F(CSurfacePassElementTest, DeclaredOpacityStillRequiresMatchingSurfaceSize) {
    auto renderData    = data();
    renderData.w       = 99.5;
    renderData.texture = nullptr;
    CSurfacePassElement mismatched{renderData};
    EXPECT_TRUE(mismatched.opaqueRegion(m_ctx).empty());

    renderData.texture = m_texture;
    CSurfacePassElement fallback{renderData};
    EXPECT_FALSE(fallback.opaqueRegion(m_ctx).empty());

    m_surface->m_current.opaque.clear();
    renderData.w = 100;
    CSurfacePassElement matching{renderData};
    EXPECT_TRUE(matching.opaqueRegion(m_ctx).empty());
}

TEST_F(CSurfacePassElementTest, RoundingCanConsumeAnOpaqueRectangle) {
    m_surface->m_current.size   = {2, 2};
    m_surface->m_current.opaque = CBox{0, 0, 2, 2};
    for (const double width : {1.6, 2.0}) {
        auto renderData     = data();
        renderData.w        = width;
        renderData.h        = 2;
        renderData.rounding = 2;
        CSurfacePassElement element{renderData};
        EXPECT_TRUE(element.opaqueRegion(m_ctx).empty());
    }
}

TEST_F(CSurfacePassElementTest, ConservativeBoundsRetainTheLastDrawnColumn) {
    m_monitor->m_scale = 1.5F;
    for (const double origin : {0.0, 1920.0}) {
        m_monitor->m_position = {origin, 0.0};
        auto renderData       = data();
        renderData.pos        = m_monitor->m_position + Vector2D{2.0 / 1.5, 2.0 / 1.5};
        renderData.w          = 3836 * (1.0 / 1.5);
        renderData.h          = 2156 * (1.0 / 1.5);
        CSurfacePassElement element{renderData};
        const auto          bounds = element.boundingBox(m_ctx);
        ASSERT_TRUE(bounds);
        EXPECT_EQ(bounds->copy().scale(m_monitor->m_scale).round(), CBox(2, 2, 3836, 2156));

        // The bounds conversion used by simplify must retain damage in column 3837.
        const CRegion pixelBounds{Render::pixelBoxForLogical(*bounds, m_monitor->m_scale)};
        CRegion       lastColumn{3837, 2, 1, 2156};
        EXPECT_FALSE(lastColumn.copy().intersect(pixelBounds).empty());
        EXPECT_TRUE(lastColumn.subtract(pixelBounds).empty());
    }
}

TEST_F(CSurfacePassElementTest, VisibleRegionUsesRoundedTextureOriginOnOffsetMonitor) {
    m_monitor->m_scale           = 1.25F;
    m_monitor->m_position        = {1920, 0};
    m_hlSurface->m_visibleRegion = CBox{10, 10, 20, 20};
    auto renderData              = data();
    renderData.w                 = 80;
    renderData.h                 = 64;
    CSurfacePassElement element{renderData};
    EXPECT_EQ(element.getTexBox().scale(m_monitor->m_scale).round(), CBox(1, 1, 100, 80));

    bool cancel  = false;
    auto visible = element.visibleRegion(m_ctx, cancel);
    EXPECT_FALSE(cancel);
    EXPECT_EQ(visible.getExtents(), CBox(10, 10, 22, 22));
}
