#include <keybinds/InputState.hpp>

#include <gtest/gtest.h>

using namespace Keybinds;

class CTestHID final : public IHID {
  public:
    uint32_t getCapabilities() override {
        return HID_INPUT_CAPABILITY_KEYBOARD;
    }
};

TEST(KeybindsInputState, TracksSameKeyPerDevice) {
    CInputState state;
    const auto  FIRST  = makeShared<CTestHID>();
    const auto  SECOND = makeShared<CTestHID>();
    const auto  KEY    = SResolvedKey{.sym = XKB_KEY_k, .code = 45};

    EXPECT_TRUE(state.press({.key = KEY, .device = FIRST}));
    EXPECT_TRUE(state.press({.key = KEY, .device = SECOND}));
    EXPECT_EQ(state.pressed().size(), 2);
    EXPECT_EQ(state.heldKeys().size(), 1);

    const auto RELEASED = state.release(KEY, FIRST);
    ASSERT_TRUE(RELEASED.has_value());
    EXPECT_EQ(RELEASED->device.lock(), FIRST);
    EXPECT_EQ(state.pressed().size(), 1);
    EXPECT_EQ(state.heldKeys().size(), 1);
    EXPECT_TRUE(state.isKeycodeDown(45));

    EXPECT_TRUE(state.release(KEY, SECOND).has_value());
    EXPECT_TRUE(state.heldKeys().empty());
}

TEST(KeybindsInputState, IgnoresDuplicatePress) {
    CInputState state;
    const auto  DEVICE = makeShared<CTestHID>();
    const auto  KEY    = SResolvedKey{.sym = XKB_KEY_k, .code = 45};

    EXPECT_TRUE(state.press({.key = KEY, .device = DEVICE}));
    EXPECT_FALSE(state.press({.key = KEY, .device = DEVICE}));
    EXPECT_EQ(state.pressed().size(), 1);
}

TEST(KeybindsInputState, DoesNotRetainDevice) {
    CInputState state;
    auto        device = makeShared<CTestHID>();
    const auto  KEY    = SResolvedKey{.sym = XKB_KEY_k, .code = 45};
    WP<IHID>    weak   = device;

    EXPECT_TRUE(state.press({.key = KEY, .device = device}));
    device.reset();

    EXPECT_TRUE(weak.expired());
    EXPECT_TRUE(state.pressed().front().device.expired());
}

TEST(KeybindsInputState, PreservesDifferentSymbolsForSameCode) {
    CInputState state;
    const auto  FIRST  = makeShared<CTestHID>();
    const auto  SECOND = makeShared<CTestHID>();

    EXPECT_TRUE(state.press({.key = {.sym = XKB_KEY_k, .code = 45}, .device = FIRST}));
    EXPECT_TRUE(state.press({.key = {.sym = XKB_KEY_a, .code = 45}, .device = SECOND}));
    EXPECT_EQ(state.heldKeys().size(), 2);
}

TEST(KeybindsInputState, PreservesPressMetadata) {
    CInputState state;
    const auto  DEVICE = makeShared<CTestHID>();
    const auto  KEY    = SResolvedKey{.sym = XKB_KEY_k, .code = 45};

    state.press({
        .key              = KEY,
        .modifiersAtPress = HL_MODIFIER_META,
        .forwarded        = false,
        .submapAtPress    = "resize",
        .positionAtPress  = {10, 20},
        .device           = DEVICE,
    });

    const auto RELEASED = state.release(KEY, DEVICE);
    ASSERT_TRUE(RELEASED.has_value());
    EXPECT_EQ(RELEASED->modifiersAtPress, HL_MODIFIER_META);
    EXPECT_FALSE(RELEASED->forwarded);
    EXPECT_EQ(RELEASED->submapAtPress, "resize");
    EXPECT_EQ(RELEASED->positionAtPress, Vector2D(10, 20));
}

TEST(KeybindsInputState, ClearsOneDevice) {
    CInputState state;
    const auto  FIRST  = makeShared<CTestHID>();
    const auto  SECOND = makeShared<CTestHID>();

    state.press({.key = {.sym = XKB_KEY_k, .code = 45}, .device = FIRST});
    state.press({.key = {.sym = XKB_KEY_a, .code = 38}, .device = SECOND});
    state.clearDevice(FIRST);

    EXPECT_FALSE(state.isKeycodeDown(45));
    EXPECT_TRUE(state.isKeycodeDown(38));
    EXPECT_EQ(state.heldKeys().size(), 1);
}

TEST(KeybindsInputState, TakesReleaseCallbacksWithOriginalActionContext) {
    CInputState state;
    const auto  DEVICE = makeShared<CTestHID>();
    const auto  KEY    = SResolvedKey{.sym = XKB_KEY_k, .code = 45};
    auto        made   = CBind::make({"K"}, 0, [] { return SBindResult{}; });
    ASSERT_TRUE(made.has_value());
    const auto BIND = makeShared<CBind>(std::move(*made));

    state.press({.key = KEY, .actionCode = 45, .actionTimeMs = 123, .device = DEVICE});
    state.find(KEY, DEVICE)->releaseCallbacks.emplace_back(BIND);

    const auto RELEASES = state.takeReleaseCallbacks(KEY, DEVICE);
    ASSERT_EQ(RELEASES.size(), 1);
    EXPECT_EQ(RELEASES.front().bind, BIND);
    EXPECT_EQ(RELEASES.front().actionCode, 45);
    EXPECT_EQ(RELEASES.front().actionTimeMs, 123);
    EXPECT_TRUE(state.find(KEY, DEVICE)->releaseCallbacks.empty());
}
