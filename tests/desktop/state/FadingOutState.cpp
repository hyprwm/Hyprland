#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <desktop/state/FadingOutState.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <output/Monitor.hpp>
#include <protocols/types/Buffer.hpp>
#include <render/Framebuffer.hpp>
#include <render/Renderer.hpp>
#include <render/scene/SceneSelection.hpp>

#include <gtest/gtest.h>

#include <utility>

using namespace Desktop;
using namespace Render;

class CFadingOutTestOutput : public Aquamarine::IOutput {
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

class CFadingOutTestFramebuffer : public IFramebuffer {
  public:
    void release() override {
        ;
    }
    bool readPixels(CHLBufferReference, uint32_t, uint32_t, uint32_t, uint32_t) override {
        ADD_FAILURE() << "Unexpected snapshot readback";
        return false;
    }
    void bind() override {
        ADD_FAILURE() << "Unexpected snapshot bind";
    }
    void addStencil(SP<ITexture>) override {
        ADD_FAILURE() << "Unexpected stencil allocation";
    }

  private:
    bool internalAlloc(int, int, DRMFormat) override {
        ADD_FAILURE() << "Unexpected snapshot allocation";
        return false;
    }
};

class CFadingOutTestFadeout : public IFadeout {
  public:
    explicit CFadingOutTestFadeout(PHLMONITORREF monitor) : m_monitor(monitor) {
        m_framebuffer          = makeShared<CFadingOutTestFramebuffer>();
        m_workspaceFramebuffer = makeShared<CFadingOutTestFramebuffer>();
        m_source.type          = eFadeoutSource::WINDOW;
    }

    PHLMONITORREF monitor() const override {
        return m_monitor;
    }
    eFadeoutPlane plane() const override {
        return FADEOUT_PLANE_WINDOW_FLOATING;
    }
    int zIndex() const override {
        return 0;
    }
    CBox renderBox() const override {
        return {};
    }
    float alpha() const override {
        return m_alpha;
    }
    bool done() const override {
        return m_done;
    }

    bool  m_done  = false;
    float m_alpha = 0.5F;

  private:
    PHLMONITORREF m_monitor;
};

class CFadingOutStateTest : public ::testing::Test {
  protected:
    void       SetUp() override;
    void       TearDown() override;

    PHLMONITOR m_monitor;

  private:
    UP<CCompositor>                      m_previousCompositor;
    UP<CEventLoopManager>                m_previousEventLoop;
    UP<IHyprRenderer>                    m_previousRenderer;
    UP<Event::CEventBus>                 m_previousEventBus;
    UP<Config::IConfigManager>           m_previousConfig;
    UP<Config::CConfigWatcher>           m_previousWatcher;
    UP<Animation::CHyprAnimationManager> m_previousAnimations;
    UP<Config::CAnimationTreeController> m_previousTree;
};

void CFadingOutStateTest::SetUp() {
    m_previousCompositor = std::move(g_pCompositor);
    m_previousEventLoop  = std::move(g_pEventLoopManager);
    m_previousRenderer   = std::move(g_pHyprRenderer);
    m_previousEventBus   = std::move(Event::bus());
    m_previousConfig     = std::move(Config::mgr());
    m_previousWatcher    = std::move(Config::watcher());
    m_previousAnimations = std::move(Animation::mgr());
    m_previousTree       = std::move(Config::animationTree());

    Event::bus()  = makeUnique<Event::CEventBus>();
    g_pCompositor = makeUnique<CCompositor>(true);
    // Construct only config defaults. This dummy path is never loaded or written.
    g_pCompositor->m_explicitConfigPath = "fading-out-state-test.lua";
    Config::Supplementary::Jeremy::flushCachedCfgPath();
    Config::watcher() = makeUnique<Config::CConfigWatcher>();
    Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
    CConfigValueBase::flushCaches();
    Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
    Config::animationTree() = makeUnique<Config::CAnimationTreeController>();

    // Real monitor metadata, with no renderer, event loop, or connected backend.
    auto output       = makeShared<CFadingOutTestOutput>();
    output->name      = "fading-out-state-test";
    m_monitor         = makeShared<Monitor::CMonitor>(output);
    m_monitor->m_self = m_monitor;
    m_monitor->m_size = m_monitor->m_pixelSize = m_monitor->m_transformedSize = {640, 480};
}

void CFadingOutStateTest::TearDown() {
    m_monitor.reset();
    Animation::mgr().reset();
    Config::animationTree().reset();
    Config::mgr().reset();
    Config::watcher().reset();
    g_pCompositor.reset();
    Event::bus().reset();

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

TEST_F(CFadingOutStateTest, NullAdditionPreservesExistingFadeouts) {
    CFadingOutState state;
    state.add(nullptr);
    EXPECT_TRUE(state.fadeouts().empty());

    auto fadeout = makeShared<CFadingOutTestFadeout>(m_monitor);
    state.add(fadeout);
    state.add(nullptr);
    ASSERT_EQ(state.fadeouts().size(), 1U);
    EXPECT_EQ(state.fadeouts().front(), fadeout);
}

TEST_F(CFadingOutStateTest, FinishedAdditionReleasesBothSnapshotsWithoutTick) {
    CFadingOutState state;
    for (const float alpha : {0.F, 0.5F}) {
        SCOPED_TRACE(alpha);
        auto fadeout     = makeShared<CFadingOutTestFadeout>(m_monitor);
        fadeout->m_done  = true;
        fadeout->m_alpha = alpha;

        WP<IFadeout>     weakFadeout       = fadeout;
        WP<IFramebuffer> monitorSnapshot   = fadeout->framebuffer();
        WP<IFramebuffer> workspaceSnapshot = fadeout->framebuffer(eSceneMode::WORKSPACE_WINDOWS);
        ASSERT_FALSE(monitorSnapshot.expired());
        ASSERT_FALSE(workspaceSnapshot.expired());

        state.add(fadeout);
        fadeout.reset();
        EXPECT_TRUE(state.fadeouts().empty());
        EXPECT_TRUE(weakFadeout.expired());
        EXPECT_TRUE(monitorSnapshot.expired());
        EXPECT_TRUE(workspaceSnapshot.expired());
    }
}

TEST_F(CFadingOutStateTest, ExpiredMonitorAdditionReleasesBothSnapshotsWithoutTick) {
    CFadingOutState  state;
    auto             fadeout           = makeShared<CFadingOutTestFadeout>(m_monitor);
    WP<IFadeout>     weakFadeout       = fadeout;
    WP<IFramebuffer> monitorSnapshot   = fadeout->framebuffer();
    WP<IFramebuffer> workspaceSnapshot = fadeout->framebuffer(eSceneMode::WORKSPACE_WINDOWS);
    ASSERT_FALSE(monitorSnapshot.expired());
    ASSERT_FALSE(workspaceSnapshot.expired());
    ASSERT_FALSE(fadeout->done());

    m_monitor.reset();
    ASSERT_TRUE(fadeout->monitor().expired());
    state.add(fadeout);
    fadeout.reset();
    EXPECT_TRUE(state.fadeouts().empty());
    EXPECT_TRUE(weakFadeout.expired());
    EXPECT_TRUE(monitorSnapshot.expired());
    EXPECT_TRUE(workspaceSnapshot.expired());
}

TEST_F(CFadingOutStateTest, UnfinishedAdditionRetainsBothSnapshotsEvenWhenTransparent) {
    CFadingOutState state;
    for (const float alpha : {0.F, 0.5F}) {
        SCOPED_TRACE(alpha);
        auto fadeout     = makeShared<CFadingOutTestFadeout>(m_monitor);
        fadeout->m_alpha = alpha;
        ASSERT_FALSE(fadeout->done());

        WP<IFadeout>     weakFadeout       = fadeout;
        WP<IFramebuffer> monitorSnapshot   = fadeout->framebuffer();
        WP<IFramebuffer> workspaceSnapshot = fadeout->framebuffer(eSceneMode::WORKSPACE_WINDOWS);
        state.add(fadeout);
        fadeout.reset();
        ASSERT_EQ(state.fadeouts().size(), 1U);
        EXPECT_EQ(state.fadeouts().front(), weakFadeout.lock());
        EXPECT_FALSE(weakFadeout.expired());
        EXPECT_FALSE(monitorSnapshot.expired());
        EXPECT_FALSE(workspaceSnapshot.expired());

        state.clear();
        EXPECT_TRUE(weakFadeout.expired());
        EXPECT_TRUE(monitorSnapshot.expired());
        EXPECT_TRUE(workspaceSnapshot.expired());
    }
}
