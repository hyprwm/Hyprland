#include <render/decorations/AnimatedDecorationGradient.hpp>
#include <animation/AnimationManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>

#include <gtest/gtest.h>
#include <numbers>

static Config::CGradientValueData gradient(float angle, bool wholeMonitor = false) {
    Config::CGradientValueData result({CHyprColor(0xFFFF0000), CHyprColor(0xFF0000FF)}, angle);
    result.m_wholeMonitor = wholeMonitor;
    return result;
}

class CAnimatedDecorationGradientTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_previousAnimations    = std::move(Animation::mgr());
        m_previousTree          = std::move(Config::animationTree());
        Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree() = makeUnique<Config::CAnimationTreeController>();
        Config::animationTree()->setConfigForNode("borderangle", true, 1.F, "linear", "loop");
        Config::animationTree()->setConfigForNode("border", true, 1.F, "linear");
    }

    void TearDown() override {
        m_gradient.reset();
        Animation::mgr()        = std::move(m_previousAnimations);
        Config::animationTree() = std::move(m_previousTree);
    }

    void initialize(const Config::CGradientValueData& initial) {
        m_gradient = makeUnique<CAnimatedDecorationGradient>(initial);
        m_gradient->initializeAnimations(nullptr, nullptr, "border", "borderangle");
    }

    PHLANIMVARREF<float> activeAnimation(const std::string& name) {
        const auto CONFIG = Config::animationTree()->getAnimationPropertyConfig(name);
        for (const auto& animation : Animation::mgr()->m_vActiveAnimatedVariables) {
            if (animation && animation->getConfig() == CONFIG)
                return dynamicPointerCast<CAnimatedVariable<float>>(animation);
        }
        return {};
    }

    UP<CAnimatedDecorationGradient> m_gradient;

  private:
    UP<Animation::CHyprAnimationManager> m_previousAnimations;
    UP<Config::CAnimationTreeController> m_previousTree;
};

TEST_F(CAnimatedDecorationGradientTest, MonitorGradientDoesNotStartOnMapOrFocus) {
    initialize(gradient(0.7F, true));
    m_gradient->onWindowMap();
    m_gradient->onWindowFocus();

    EXPECT_FALSE(Animation::mgr()->shouldTickForNext());
    EXPECT_FLOAT_EQ(m_gradient->renderState().current.m_angle, 0.7F);
}

TEST_F(CAnimatedDecorationGradientTest, FlagOnlyChangeStopsLoopAndFreezesOutgoingAngle) {
    initialize(gradient(0.7F));
    m_gradient->onWindowMap();
    const auto ANGLE = activeAnimation("borderangle");
    ASSERT_TRUE(ANGLE);
    ANGLE->value()            = 0.25F;
    const auto OUTGOING_ANGLE = m_gradient->renderState().current.m_angle;

    m_gradient->setTarget(gradient(0.7F, true));
    const auto STATE = m_gradient->renderState();
    EXPECT_TRUE(STATE.transitioning);
    EXPECT_TRUE(STATE.current.m_wholeMonitor);
    EXPECT_FLOAT_EQ(STATE.current.m_angle, 0.7F);
    EXPECT_FLOAT_EQ(STATE.previous.m_angle, OUTGOING_ANGLE);
    EXPECT_FALSE(ANGLE->isBeingAnimated());
    EXPECT_TRUE(ANGLE->enabled());
    EXPECT_FALSE(activeAnimation("borderangle"));

    const auto FADE = activeAnimation("border");
    ASSERT_TRUE(FADE);
    FADE->warp();
    ANGLE->onAnimationEnd();
    m_gradient->onWindowFocus();
    EXPECT_FALSE(Animation::mgr()->shouldTickForNext());
}

TEST_F(CAnimatedDecorationGradientTest, ReturningToLocalRestartsLoopWithoutRotatingMonitorEndpoint) {
    initialize(gradient(0.7F, true));
    m_gradient->onWindowMap();
    m_gradient->setTarget(gradient(0.2F));

    const auto ANGLE = activeAnimation("borderangle");
    ASSERT_TRUE(ANGLE);
    ANGLE->value()   = 0.25F;
    const auto STATE = m_gradient->renderState();
    EXPECT_TRUE(STATE.transitioning);
    EXPECT_TRUE(STATE.previous.m_wholeMonitor);
    EXPECT_FLOAT_EQ(STATE.previous.m_angle, 0.7F);
    EXPECT_NEAR(STATE.current.m_angle, 0.2F + std::numbers::pi_v<float> / 2.F, 1e-6);

    ANGLE->warp();
    EXPECT_TRUE(ANGLE->isBeingAnimated());
    EXPECT_FLOAT_EQ(ANGLE->value(), 0.F);
}

TEST_F(CAnimatedDecorationGradientTest, MonitorColorFadePreservesBothConfiguredAngles) {
    initialize(gradient(0.7F, true));
    m_gradient->onWindowMap();
    m_gradient->setTarget(gradient(1.4F, true));

    const auto FADE = activeAnimation("border");
    ASSERT_TRUE(FADE);
    FADE->value()    = 0.5F;
    const auto STATE = m_gradient->renderState();
    EXPECT_TRUE(STATE.transitioning);
    EXPECT_FLOAT_EQ(STATE.progress, 0.5F);
    EXPECT_FLOAT_EQ(STATE.previous.m_angle, 0.7F);
    EXPECT_FLOAT_EQ(STATE.current.m_angle, 1.4F);
    EXPECT_FALSE(activeAnimation("borderangle"));
}

TEST_F(CAnimatedDecorationGradientTest, LocalColorFadesKeepExistingSharedAnimatedAngle) {
    initialize(gradient(0.7F));
    m_gradient->onWindowMap();
    const auto ANGLE = activeAnimation("borderangle");
    ASSERT_TRUE(ANGLE);
    ANGLE->value() = 0.25F;
    m_gradient->setTarget(gradient(1.4F));

    const auto STATE = m_gradient->renderState();
    EXPECT_TRUE(STATE.transitioning);
    EXPECT_NEAR(STATE.current.m_angle, 1.4F + std::numbers::pi_v<float> / 2.F, 1e-6);
    EXPECT_FLOAT_EQ(STATE.previous.m_angle, STATE.current.m_angle);
    EXPECT_TRUE(ANGLE->isBeingAnimated());
}

TEST_F(CAnimatedDecorationGradientTest, ImmediateChangesStopAndRestartAngleWithoutAFade) {
    initialize(gradient(0.7F));
    m_gradient->onWindowMap();
    m_gradient->setTarget(gradient(0.7F, true), false);
    EXPECT_FALSE(Animation::mgr()->shouldTickForNext());
    EXPECT_FALSE(m_gradient->renderState().transitioning);

    m_gradient->setTarget(gradient(0.7F), false);
    EXPECT_TRUE(activeAnimation("borderangle"));
    EXPECT_FALSE(activeAnimation("border"));
}

TEST_F(CAnimatedDecorationGradientTest, ReturningToLocalRespectsDisabledAngleConfig) {
    initialize(gradient(0.7F, true));
    m_gradient->onWindowMap();
    Config::animationTree()->setConfigForNode("borderangle", false, 1.F, "linear", "loop");
    m_gradient->setTarget(gradient(0.7F), false);
    m_gradient->onWindowFocus();

    EXPECT_FALSE(Animation::mgr()->shouldTickForNext());
    EXPECT_FLOAT_EQ(m_gradient->renderState().current.m_angle, 0.7F);
}
