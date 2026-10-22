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
#include <state/MonitorState.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

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

class CAnimatedFadingOutTestFadeout : public CFadingOutTestFadeout {
  public:
    CAnimatedFadingOutTestFadeout(PHLMONITORREF monitor, std::vector<std::string>& events) : CFadingOutTestFadeout(monitor), m_events(events) {
        Animation::mgr()->createAnimation(1.F, m_animatedAlpha, Config::animationTree()->getAnimationPropertyConfig("fadeOut"), AVARDAMAGE_NONE);
        *m_animatedAlpha = 0.F;
    }
    ~CAnimatedFadingOutTestFadeout() override {
        m_events.emplace_back("destroy");
    }

    float alpha() const override {
        return m_animatedAlpha->value();
    }
    bool done() const override {
        return alpha() <= 0.F && !m_animatedAlpha->isBeingAnimated();
    }

    PHLANIMVAR<float> m_animatedAlpha;

  private:
    std::vector<std::string>& m_events;
};

class CFadingOutStateTest : public ::testing::Test {
  protected:
    void       SetUp() override;
    void       TearDown() override;
    PHLMONITOR makeMonitor(const std::string& name);

    PHLMONITOR m_monitor;

  private:
    UP<CCompositor>                      m_previousCompositor;
    UP<CEventLoopManager>                m_previousEventLoop;
    UP<IHyprRenderer>                    m_previousRenderer;
    UP<State::CMonitorStateTracker>      m_previousMonitors;
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
    m_previousMonitors   = std::move(State::monitorState());
    m_previousEventBus   = std::move(Event::bus());
    m_previousConfig     = std::move(Config::mgr());
    m_previousWatcher    = std::move(Config::watcher());
    m_previousAnimations = std::move(Animation::mgr());
    m_previousTree       = std::move(Config::animationTree());

    Event::bus()          = makeUnique<Event::CEventBus>();
    State::monitorState() = makeUnique<State::CMonitorStateTracker>();
    g_pCompositor         = makeUnique<CCompositor>(true);
    // Construct only config defaults. This dummy path is never loaded or written.
    g_pCompositor->m_explicitConfigPath = "fading-out-state-test.lua";
    Config::Supplementary::Jeremy::flushCachedCfgPath();
    Config::watcher() = makeUnique<Config::CConfigWatcher>();
    Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
    CConfigValueBase::flushCaches();
    Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
    Config::animationTree() = makeUnique<Config::CAnimationTreeController>();

    m_monitor = makeMonitor("fading-out-state-test");
}

void CFadingOutStateTest::TearDown() {
    State::monitorState().reset();
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
    g_pCompositor         = std::move(m_previousCompositor);
    g_pEventLoopManager   = std::move(m_previousEventLoop);
    g_pHyprRenderer       = std::move(m_previousRenderer);
    Event::bus()          = std::move(m_previousEventBus);
    State::monitorState() = std::move(m_previousMonitors);
    Config::Supplementary::Jeremy::flushCachedCfgPath();
}

PHLMONITOR CFadingOutStateTest::makeMonitor(const std::string& name) {
    // Real monitor metadata, with no renderer, event loop, or connected backend.
    auto output        = makeShared<CFadingOutTestOutput>();
    output->name       = name;
    auto monitor       = makeShared<Monitor::CMonitor>(output);
    monitor->m_self    = monitor;
    monitor->m_enabled = true;
    monitor->m_size = monitor->m_pixelSize = monitor->m_transformedSize = {640, 480};
    return monitor;
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

TEST_F(CFadingOutStateTest, TickRepeatedlyReleasesSnapshotsAcrossDpmsOffAndMixedOutputs) {
    CFadingOutState state;
    auto            other   = makeMonitor("other");
    m_monitor->m_dpmsStatus = false;

    for (const bool otherDpms : {false, true}) {
        SCOPED_TRACE(otherDpms);
        other->m_dpmsStatus = otherDpms;
        for (int cycle = 0; cycle < 3; ++cycle) {
            SCOPED_TRACE(cycle);
            auto                                  first     = makeShared<CFadingOutTestFadeout>(m_monitor);
            auto                                  second    = makeShared<CFadingOutTestFadeout>(other);
            const std::array<WP<IFramebuffer>, 4> snapshots = {
                first->framebuffer(),
                first->framebuffer(eSceneMode::WORKSPACE_WINDOWS),
                second->framebuffer(),
                second->framebuffer(eSceneMode::WORKSPACE_WINDOWS),
            };
            state.add(first);
            state.add(second);
            first->m_done = second->m_done = true;
            first.reset();
            second.reset();
            ASSERT_EQ(state.fadeouts().size(), 2U);
            for (const auto& snapshot : snapshots) {
                ASSERT_FALSE(snapshot.expired());
            }

            Event::bus()->m_events.tick.emit();
            EXPECT_TRUE(state.fadeouts().empty());
            for (const auto& snapshot : snapshots) {
                EXPECT_TRUE(snapshot.expired());
            }
        }
    }
}

TEST_F(CFadingOutStateTest, TickRetainsUnfinishedSnapshotsEvenWhenTransparent) {
    CFadingOutState state;
    for (const float alpha : {0.F, 0.5F}) {
        SCOPED_TRACE(alpha);
        auto fadeout                                = makeShared<CFadingOutTestFadeout>(m_monitor);
        fadeout->m_alpha                            = alpha;
        WP<CFadingOutTestFadeout> weakFadeout       = fadeout;
        WP<IFramebuffer>          monitorSnapshot   = fadeout->framebuffer();
        WP<IFramebuffer>          workspaceSnapshot = fadeout->framebuffer(eSceneMode::WORKSPACE_WINDOWS);
        state.add(fadeout);
        fadeout.reset();

        auto             finished         = makeShared<CFadingOutTestFadeout>(m_monitor);
        WP<IFramebuffer> finishedSnapshot = finished->framebuffer();
        state.add(finished);
        finished->m_done = true;
        finished.reset();
        Event::bus()->m_events.tick.emit();
        Event::bus()->m_events.tick.emit();
        ASSERT_EQ(state.fadeouts().size(), 1U);
        EXPECT_TRUE(finishedSnapshot.expired());
        ASSERT_FALSE(weakFadeout.expired());
        EXPECT_FALSE(monitorSnapshot.expired());
        EXPECT_FALSE(workspaceSnapshot.expired());

        weakFadeout->m_done = true;
        Event::bus()->m_events.tick.emit();
        EXPECT_TRUE(state.fadeouts().empty());
        EXPECT_TRUE(weakFadeout.expired());
        EXPECT_TRUE(monitorSnapshot.expired());
        EXPECT_TRUE(workspaceSnapshot.expired());
    }
}

TEST_F(CFadingOutStateTest, TickCollectsSnapshotsWhenMonitorExpiresAfterAddition) {
    CFadingOutState  state;
    auto             fadeout           = makeShared<CFadingOutTestFadeout>(m_monitor);
    WP<IFadeout>     weakFadeout       = fadeout;
    WP<IFramebuffer> monitorSnapshot   = fadeout->framebuffer();
    WP<IFramebuffer> workspaceSnapshot = fadeout->framebuffer(eSceneMode::WORKSPACE_WINDOWS);
    state.add(fadeout);
    fadeout.reset();

    m_monitor.reset();
    ASSERT_FALSE(weakFadeout.expired());
    ASSERT_TRUE(weakFadeout->monitor().expired());
    ASSERT_FALSE(weakFadeout->done());
    ASSERT_FALSE(monitorSnapshot.expired());
    ASSERT_FALSE(workspaceSnapshot.expired());
    Event::bus()->m_events.tick.emit();
    EXPECT_TRUE(state.fadeouts().empty());
    EXPECT_TRUE(weakFadeout.expired());
    EXPECT_TRUE(monitorSnapshot.expired());
    EXPECT_TRUE(workspaceSnapshot.expired());
}

TEST_F(CFadingOutStateTest, ClearReleasesSnapshotsAndPreservesTickCleanupOnReuse) {
    CFadingOutState state;
    for (const bool clearImmediately : {true, false}) {
        SCOPED_TRACE(clearImmediately);
        auto                      fadeout           = makeShared<CFadingOutTestFadeout>(m_monitor);
        WP<CFadingOutTestFadeout> weakFadeout       = fadeout;
        WP<IFramebuffer>          monitorSnapshot   = fadeout->framebuffer();
        WP<IFramebuffer>          workspaceSnapshot = fadeout->framebuffer(eSceneMode::WORKSPACE_WINDOWS);
        state.add(fadeout);
        fadeout.reset();
        ASSERT_FALSE(weakFadeout.expired());
        ASSERT_FALSE(monitorSnapshot.expired());
        ASSERT_FALSE(workspaceSnapshot.expired());

        if (clearImmediately)
            state.clear();
        else {
            weakFadeout->m_done = true;
            Event::bus()->m_events.tick.emit();
        }
        EXPECT_TRUE(state.fadeouts().empty());
        EXPECT_TRUE(weakFadeout.expired());
        EXPECT_TRUE(monitorSnapshot.expired());
        EXPECT_TRUE(workspaceSnapshot.expired());
        Event::bus()->m_events.tick.emit();
        EXPECT_TRUE(state.fadeouts().empty());
    }
}

TEST_F(CFadingOutStateTest, EarlierTickListenerCanDestroyStateAndUnsubscribe) {
    UP<CFadingOutState> state;
    // Signal emission snapshots listeners before dispatching this earlier one.
    auto destroyState                  = Event::bus()->m_events.tick.listen([&] { state.reset(); });
    state                              = makeUnique<CFadingOutState>();
    auto             fadeout           = makeShared<CFadingOutTestFadeout>(m_monitor);
    WP<IFadeout>     weakFadeout       = fadeout;
    WP<IFramebuffer> monitorSnapshot   = fadeout->framebuffer();
    WP<IFramebuffer> workspaceSnapshot = fadeout->framebuffer(eSceneMode::WORKSPACE_WINDOWS);
    state->add(fadeout);
    fadeout.reset();
    ASSERT_FALSE(monitorSnapshot.expired());
    ASSERT_FALSE(workspaceSnapshot.expired());

    Event::bus()->m_events.tick.emit();
    EXPECT_FALSE(state);
    EXPECT_TRUE(weakFadeout.expired());
    EXPECT_TRUE(monitorSnapshot.expired());
    EXPECT_TRUE(workspaceSnapshot.expired());
    Event::bus()->m_events.tick.emit();
}

TEST_F(CFadingOutStateTest, MonitorFallbackFiltersCompletedEntriesButCollectsAllExpiredMonitors) {
    CFadingOutState                          state;
    auto                                     other    = makeMonitor("other");
    auto                                     expired  = makeMonitor("expired");
    std::array<SP<CFadingOutTestFadeout>, 4> fadeouts = {
        makeShared<CFadingOutTestFadeout>(m_monitor),
        makeShared<CFadingOutTestFadeout>(other),
        makeShared<CFadingOutTestFadeout>(expired),
        makeShared<CFadingOutTestFadeout>(m_monitor),
    };
    std::array<WP<IFramebuffer>, 4> snapshots;
    for (size_t i = 0; i < fadeouts.size(); ++i) {
        snapshots[i] = fadeouts[i]->framebuffer();
        state.add(fadeouts[i]);
        fadeouts[i]->m_done = i < 2;
        fadeouts[i].reset();
    }
    PHLMONITORREF expiredMonitor = expired;
    expired.reset();
    ASSERT_TRUE(expiredMonitor.expired());

    state.cleanupForMonitor(nullptr);
    ASSERT_EQ(state.fadeouts().size(), 4U);
    for (const auto& snapshot : snapshots) {
        ASSERT_FALSE(snapshot.expired());
    }

    state.cleanupForMonitor(m_monitor);
    EXPECT_EQ(state.fadeouts().size(), 2U);
    EXPECT_TRUE(snapshots[0].expired());
    EXPECT_FALSE(snapshots[1].expired()); // Finished on a different live output.
    EXPECT_TRUE(snapshots[2].expired());  // Expired outputs are always collected.
    EXPECT_FALSE(snapshots[3].expired()); // Unfinished on the selected output.

    state.cleanupForMonitor(other);
    EXPECT_EQ(state.fadeouts().size(), 1U);
    EXPECT_TRUE(snapshots[1].expired());
    EXPECT_FALSE(snapshots[3].expired());
    state.clear();
    EXPECT_TRUE(snapshots[3].expired());
}

TEST_F(CFadingOutStateTest, FrameTickReleasesSnapshotsAfterFinalAnimationUpdateOnDpmsOffOutput) {
    m_monitor->m_dpmsStatus = false;
    // Unlike tracker.add(), this only publishes metadata and never connects the output.
    Event::bus()->m_events.monitor.added.emit(m_monitor);
    ASSERT_TRUE(m_monitor->m_enabled);
    ASSERT_FALSE(g_pHyprRenderer);
    Config::animationTree()->setConfigForNode("fadeOut", false, 1.F, "linear");

    std::vector<std::string> events;
    CFadingOutState          state;
    auto                     fadeout           = makeShared<CAnimatedFadingOutTestFadeout>(m_monitor, events);
    WP<IFadeout>             weakFadeout       = fadeout;
    PHLANIMVARREF<float>     animatedAlpha     = fadeout->m_animatedAlpha;
    WP<IFramebuffer>         monitorSnapshot   = fadeout->framebuffer();
    WP<IFramebuffer>         workspaceSnapshot = fadeout->framebuffer(eSceneMode::WORKSPACE_WINDOWS);
    fadeout->m_animatedAlpha->setUpdateCallback([&](auto) {
        ASSERT_FALSE(weakFadeout.expired());
        EXPECT_FLOAT_EQ(weakFadeout->alpha(), 0.F);
        EXPECT_FALSE(monitorSnapshot.expired());
        EXPECT_FALSE(workspaceSnapshot.expired());
        events.emplace_back("update");
    });
    ASSERT_FALSE(fadeout->done());
    ASSERT_TRUE(fadeout->m_animatedAlpha->isBeingAnimated());
    state.add(fadeout);
    fadeout.reset();
    auto afterCleanup = Event::bus()->m_events.tick.listen([&] {
        EXPECT_TRUE(state.fadeouts().empty());
        EXPECT_TRUE(weakFadeout.expired());
        EXPECT_TRUE(animatedAlpha.expired());
        EXPECT_TRUE(monitorSnapshot.expired());
        EXPECT_TRUE(workspaceSnapshot.expired());
        events.emplace_back("tick");
    });

    // Session inactivity pauses animation and reclamation together.
    g_pCompositor->m_sessionActive = false;
    Animation::mgr()->resetTickState();
    Animation::mgr()->requestTick();
    Animation::mgr()->frameTick();
    EXPECT_TRUE(events.empty());
    ASSERT_EQ(state.fadeouts().size(), 1U);
    EXPECT_FALSE(monitorSnapshot.expired());
    EXPECT_FALSE(workspaceSnapshot.expired());

    g_pCompositor->m_sessionActive = true;
    Animation::mgr()->resetTickState();
    Animation::mgr()->requestTick();
    Animation::mgr()->frameTick();
    EXPECT_EQ(events, (std::vector<std::string>{"update", "destroy", "tick"}));
    EXPECT_TRUE(state.fadeouts().empty());
    EXPECT_TRUE(weakFadeout.expired());
    EXPECT_TRUE(animatedAlpha.expired());
    EXPECT_TRUE(monitorSnapshot.expired());
    EXPECT_TRUE(workspaceSnapshot.expired());
    EXPECT_FALSE(Animation::mgr()->shouldTickForNext());
}
