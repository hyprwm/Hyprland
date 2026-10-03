#include <render/gl/blur/BlurAnimationClock.hpp>
#include <render/gl/blur/Drops.hpp>
#include <render/gl/blur/HeatShimmer.hpp>
#include <render/gl/blur/Aurora.hpp>
#include <render/Context.hpp>
#include <render/SceneResources.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <Compositor.hpp>

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace Render;
using namespace Render::GL;
using namespace std::chrono_literals;

TEST(BlurAnimationClock, StartsStoppedAndIntegratesPreviousSpeed) {
    const Time::steady_tp start{};
    CBlurAnimationClock   clock(start);
    EXPECT_DOUBLE_EQ(clock.sample(start + 10s), 0.0);
    EXPECT_DOUBLE_EQ(clock.update(start + 10s, 2.F), 0.0);
    EXPECT_DOUBLE_EQ(clock.sample(start + 13s), 6.0);
    EXPECT_DOUBLE_EQ(clock.update(start + 13s, 0.5F), 6.0);
    EXPECT_DOUBLE_EQ(clock.update(start + 17s, 3.F), 8.0);
    EXPECT_DOUBLE_EQ(clock.sample(start + 19s), 14.0);
}

TEST(BlurAnimationClock, ZeroSpeedPausesWithoutLosingAccumulatedTime) {
    const Time::steady_tp start{};
    CBlurAnimationClock   clock(start);
    clock.update(start, 2.F);
    EXPECT_DOUBLE_EQ(clock.update(start + 3s, 0.F), 6.0);
    EXPECT_DOUBLE_EQ(clock.sample(start + 100s), 6.0);
    EXPECT_DOUBLE_EQ(clock.update(start + 100s, 0.5F), 6.0);
    EXPECT_DOUBLE_EQ(clock.sample(start + 104s), 8.0);
}

TEST(BlurAnimationClock, RepeatedAndOutOfOrderCaptureSamplesDoNotAffectLiveUpdates) {
    const Time::steady_tp start{};
    CBlurAnimationClock   clock(start), liveOnly(start);
    clock.update(start, 2.F);
    liveOnly.update(start, 2.F);
    for (int i = 0; i < 3; ++i) {
        EXPECT_DOUBLE_EQ(clock.sample(start + 10s), 20.0);
        EXPECT_DOUBLE_EQ(clock.sample(start + 2s), 4.0);
        EXPECT_DOUBLE_EQ(clock.sample(start - 1s), 0.0);
    }
    EXPECT_DOUBLE_EQ(clock.update(start + 5s, 3.F), liveOnly.update(start + 5s, 3.F));
    EXPECT_DOUBLE_EQ(clock.sample(start + 10s), 25.0);
    EXPECT_DOUBLE_EQ(clock.update(start + 12s, 0.F), liveOnly.update(start + 12s, 0.F));
}

TEST(BlurAnimationClock, CaptureBeforeConfigHookAnchorClampsWithoutChangingSpeedOrAnchor) {
    const Time::steady_tp start{};
    CBlurAnimationClock   clock(start);
    clock.update(start, 2.F);
    const auto capture = start + 3s;
    EXPECT_DOUBLE_EQ(clock.update(capture + 1ms, 4.F), 6.002);
    for (int i = 0; i < 3; ++i)
        EXPECT_DOUBLE_EQ(clock.sample(capture), 6.002);
    EXPECT_DOUBLE_EQ(clock.update(capture + 1001ms, 1.F), 10.002);
    EXPECT_DOUBLE_EQ(clock.sample(capture + 2001ms), 11.002);
}

namespace Render::GL {
    // Exercise the real CPU phase path used by bindFinish without a GL shader/context.
    class CBlurAnimationClockTestAccessor {
      public:
        template <typename T>
        static CBlurAnimationClock& clock(T& material) {
            return material.m_animationClock;
        }

        template <typename T>
        static float phase(const T& material, CRenderContext& ctx) {
            return material.animationPhase(ctx);
        }
    };
}

class CBlurAnimationMaterialTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Config path lookup needs the same config-only compositor used by config tests.
        m_previousCompositor                = std::move(g_pCompositor);
        g_pCompositor                       = makeUnique<CCompositor>(true);
        g_pCompositor->m_explicitConfigPath = (std::filesystem::temp_directory_path() / "hyprland-blur-animation-clock.lua").string();
        Config::Supplementary::Jeremy::flushCachedCfgPath();
        m_previousWatcher = std::move(Config::watcher());
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        m_previousConfig  = std::move(Config::mgr());
        Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();

        CConfigValue<Config::BOOL>  enabled("decoration:blur:enabled");
        CConfigValue<Config::FLOAT> refraction("decoration:blur:glass:refraction");
        *enabled.ptr()    = true;
        *refraction.ptr() = 1.F;
    }

    void TearDown() override {
        Config::mgr()     = std::move(m_previousConfig);
        Config::watcher() = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pCompositor = std::move(m_previousCompositor);
        Config::Supplementary::Jeremy::flushCachedCfgPath();
    }

    template <typename T>
    void checkSampling(T& material, const std::string& speedOption, double baseSpeed, double period, bool zeroResetsPhase, float minimumPhase = 0.F) {
        SCOPED_TRACE(speedOption);
        CRenderContext capture;
        ASSERT_TRUE(capture.begin(makeShared<CSceneResources>(SP<IFramebuffer>{})));
        const auto now = capture.effectTime();

        auto&      clock = CBlurAnimationClockTestAccessor::clock(material);
        clock            = CBlurAnimationClock(now - 10000s);
        clock.update(now - 10000s, 2.F);
        auto liveOnly = clock;

        // The configured speed deliberately differs from the live anchor's speed.
        CConfigValue<Config::FLOAT> speed(speedOption);
        *speed.ptr()        = 3.F;
        const auto expected = std::max(sc<float>(std::fmod(20000.0 * baseSpeed, period)), minimumPhase);
        for (int i = 0; i < 3; ++i) {
            EXPECT_TRUE(material.isAnimated(capture));
            EXPECT_FLOAT_EQ(CBlurAnimationClockTestAccessor::phase(material, capture), expected);
        }
        *speed.ptr() = 0.F;
        EXPECT_FALSE(material.isAnimated(capture));
        EXPECT_FLOAT_EQ(CBlurAnimationClockTestAccessor::phase(material, capture), zeroResetsPhase ? 0.F : expected);
        *speed.ptr() = 3.F;
        EXPECT_FLOAT_EQ(CBlurAnimationClockTestAccessor::phase(material, capture), expected);
        EXPECT_DOUBLE_EQ(clock.update(now + 5s, 3.F), liveOnly.update(now + 5s, 3.F));
        EXPECT_DOUBLE_EQ(clock.sample(now + 10s), liveOnly.sample(now + 10s));

        // The live/config anchor has moved beyond the capture's fixed timestamp.
        const auto anchoredPhase = std::max(sc<float>(std::fmod(20010.0 * baseSpeed, period)), minimumPhase);
        EXPECT_TRUE(material.isAnimated(capture));
        EXPECT_FLOAT_EQ(CBlurAnimationClockTestAccessor::phase(material, capture), anchoredPhase);
        EXPECT_DOUBLE_EQ(clock.update(now + 6s, 1.F), liveOnly.update(now + 6s, 1.F));

        // Retain Drops' positive minimum phase at the initial anchor.
        clock = CBlurAnimationClock(now);
        EXPECT_FLOAT_EQ(CBlurAnimationClockTestAccessor::phase(material, capture), minimumPhase);
        *speed.ptr() = 0.F;
        EXPECT_FLOAT_EQ(CBlurAnimationClockTestAccessor::phase(material, capture), 0.F);
    }

  private:
    UP<CCompositor>            m_previousCompositor;
    UP<Config::IConfigManager> m_previousConfig;
    UP<Config::CConfigWatcher> m_previousWatcher;
};

TEST_F(CBlurAnimationMaterialTest, DropsPreservesPeriodZeroSpeedAndMinimumPhaseWithoutMutation) {
    CDropsBlurMaterial material;
    checkSampling(material, "decoration:blur:drops:speed", 0.055, 256.0, true, 0.00002F);
}

TEST_F(CBlurAnimationMaterialTest, HeatShimmerPreservesPeriodAndZeroSpeedWithoutMutation) {
    CHeatShimmerBlurMaterial material;
    checkSampling(material, "decoration:blur:heat_shimmer:speed", 0.8, 6.283185307179586, true);
}

TEST_F(CBlurAnimationMaterialTest, AuroraPreservesPeriodAndPausedPhaseWithoutMutation) {
    CAuroraBlurMaterial material;
    checkSampling(material, "decoration:blur:aurora:speed", 0.22, 6.283185307179586, false);
}
