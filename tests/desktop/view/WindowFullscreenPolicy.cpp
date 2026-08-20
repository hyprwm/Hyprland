#include <desktop/view/window/WindowFullscreenPolicy.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;
using namespace Fullscreen;

TEST(WindowFullscreenPolicy, DefaultAndEffectiveStacking) {
    CWindowFullscreenPolicy policy;

    EXPECT_TRUE(policy.allowedOverFullscreen());
    EXPECT_TRUE(policy.effectiveAllowedOverFullscreen({}));

    policy.setAllowedOverFullscreen(false);
    EXPECT_FALSE(policy.effectiveAllowedOverFullscreen({}));
    EXPECT_TRUE(policy.effectiveAllowedOverFullscreen({.isFullscreenWindow = true}));
    EXPECT_TRUE(policy.effectiveAllowedOverFullscreen({.pinned = true}));
    EXPECT_TRUE(policy.effectiveAllowedOverFullscreen({.groupedWithFullscreen = true}));
}

TEST(WindowFullscreenPolicy, SuppressionDomainsAreIndependentAndReplaceable) {
    CWindowFullscreenPolicy policy;

    policy.setRequestSuppression({.fullscreen = true, .fullscreenOutput = true});
    EXPECT_TRUE(policy.requestSuppression().fullscreen);
    EXPECT_FALSE(policy.requestSuppression().maximize);
    EXPECT_TRUE(policy.requestSuppression().fullscreenOutput);

    policy.setRequestSuppression({.maximize = true});
    EXPECT_FALSE(policy.requestSuppression().fullscreen);
    EXPECT_TRUE(policy.requestSuppression().maximize);
    EXPECT_FALSE(policy.requestSuppression().fullscreenOutput);
}

TEST(WindowFullscreenPolicy, PendingRequestsDistinguishModesAndConsume) {
    CWindowFullscreenPolicy policy;

    policy.setPendingClientRequest(FSMODE_MAXIMIZED);
    EXPECT_EQ(policy.pendingClientRequest().mode, FSMODE_MAXIMIZED);
    EXPECT_EQ(policy.pendingClientRequest().monitor, std::nullopt);

    policy.clearPendingClientMode(FSMODE_FULLSCREEN);
    EXPECT_EQ(policy.pendingClientRequest().mode, FSMODE_MAXIMIZED);

    policy.setPendingClientRequest(FSMODE_FULLSCREEN, 3);
    EXPECT_EQ(policy.pendingClientRequest().mode, FSMODE_FULLSCREEN);
    EXPECT_EQ(policy.pendingClientRequest().monitor, 3);

    const auto request = policy.consumePendingClientRequest();
    EXPECT_EQ(request.mode, FSMODE_FULLSCREEN);
    EXPECT_EQ(request.monitor, 3);
    EXPECT_EQ(policy.pendingClientRequest().mode, std::nullopt);
    EXPECT_EQ(policy.pendingClientRequest().monitor, std::nullopt);
}

TEST(WindowFullscreenPolicy, ExpectedMaximizeEchoClearsOnNextRequest) {
    CWindowFullscreenPolicy policy;

    policy.expectMaximizeEcho();
    EXPECT_FALSE(policy.consumeExpectedMaximizeEcho(false));
    EXPECT_FALSE(policy.consumeExpectedMaximizeEcho(true));

    policy.expectMaximizeEcho();
    EXPECT_TRUE(policy.consumeExpectedMaximizeEcho(true));
    EXPECT_FALSE(policy.consumeExpectedMaximizeEcho(true));

    policy.expectMaximizeEcho();
    policy.clearExpectedMaximizeEcho();
    EXPECT_FALSE(policy.consumeExpectedMaximizeEcho(true));
}

TEST(WindowFullscreenPolicy, PinAndMaximizedRestorationAreIndependent) {
    CWindowFullscreenPolicy policy;

    policy.setPinFullscreened(true);
    EXPECT_TRUE(policy.pinFullscreened());
    EXPECT_FALSE(policy.restoreClientMaximized());

    policy.setRestoreClientMaximized(true);
    EXPECT_TRUE(policy.pinFullscreened());
    EXPECT_TRUE(policy.restoreClientMaximized());

    policy.setPinFullscreened(false);
    policy.setRestoreClientMaximized(false);
    EXPECT_FALSE(policy.pinFullscreened());
    EXPECT_FALSE(policy.restoreClientMaximized());
}
