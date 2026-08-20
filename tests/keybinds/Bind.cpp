#include <keybinds/Bind.hpp>

#include <gtest/gtest.h>

#include <array>

using namespace Keybinds;
using namespace Input;

static SResolvedKey resolvedKey(const char* name, xkb_keycode_t code, std::optional<Input::eKeyboardModifiers> modifier = std::nullopt) {
    return {
        .sym      = xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE),
        .code     = code,
        .modifier = modifier,
    };
}

static std::expected<CBind, std::string> makeBind(std::vector<std::string>&& keys, eBindFlags flags = sc<eBindFlags>(0)) {
    return CBind::make(std::move(keys), flags, [] { return SBindResult{}; });
}

TEST(Keybinds, SidedModifierMatchesCorrectSide) {
    auto result = makeBind({"SHIFT_L", "K"});
    ASSERT_TRUE(result.has_value());

    const auto       LEFT_SHIFT  = resolvedKey("SHIFT_L", 50, HL_MODIFIER_SHIFT);
    const auto       RIGHT_SHIFT = resolvedKey("SHIFT_R", 62, HL_MODIFIER_SHIFT);
    const auto       K           = resolvedKey("K", 45);

    const std::array leftHeld = {LEFT_SHIFT, K};
    EXPECT_EQ(result->matches({
                  .heldKeys     = leftHeld,
                  .trigger      = K,
                  .modifiersNow = HL_MODIFIER_SHIFT,
              }),
              BIND_MATCH_FULL);

    const std::array rightHeld = {RIGHT_SHIFT, K};
    EXPECT_EQ(result->matches({
                  .heldKeys     = rightHeld,
                  .trigger      = K,
                  .modifiersNow = HL_MODIFIER_SHIFT,
              }),
              BIND_MATCH_NONE);

    const std::array bothHeld = {LEFT_SHIFT, RIGHT_SHIFT, K};
    EXPECT_EQ(result->matches({
                  .heldKeys     = bothHeld,
                  .trigger      = K,
                  .modifiersNow = HL_MODIFIER_SHIFT,
              }),
              BIND_MATCH_NONE);
}

TEST(Keybinds, SidedModifierCanPartiallyMatch) {
    auto result = makeBind({"SHIFT_L", "K"});
    ASSERT_TRUE(result.has_value());

    const auto       LEFT_SHIFT = resolvedKey("SHIFT_L", 50, HL_MODIFIER_SHIFT);
    const std::array held       = {LEFT_SHIFT};

    EXPECT_EQ(result->matches({
                  .heldKeys     = held,
                  .trigger      = LEFT_SHIFT,
                  .modifiersNow = Input::HL_MODIFIER_NONE,
              }),
              BIND_MATCH_PARTIAL);
}

TEST(Keybinds, UnsidedModifierMatchesEitherSide) {
    auto result = makeBind({"SHIFT", "K"});
    ASSERT_TRUE(result.has_value());

    const auto       RIGHT_SHIFT = resolvedKey("SHIFT_R", 62, HL_MODIFIER_SHIFT);
    const auto       K           = resolvedKey("K", 45);
    const std::array held        = {RIGHT_SHIFT, K};

    EXPECT_EQ(result->matches({
                  .heldKeys     = held,
                  .trigger      = K,
                  .modifiersNow = HL_MODIFIER_SHIFT,
              }),
              BIND_MATCH_FULL);
}

TEST(Keybinds, UnsidedModifierAloneDoesNotPartiallyMatch) {
    auto result = makeBind({"SUPER", "K"});
    ASSERT_TRUE(result.has_value());

    const auto       SUPER = resolvedKey("SUPER_L", 133, HL_MODIFIER_META);
    const std::array held  = {SUPER};
    EXPECT_EQ(result->matches({.heldKeys = held, .trigger = SUPER, .modifiersNow = HL_MODIFIER_META}), BIND_MATCH_NONE);
}

TEST(Keybinds, SingleKeyAllowsOtherHeldKeys) {
    auto result = makeBind({"K"});
    ASSERT_TRUE(result.has_value());

    const auto       A    = resolvedKey("A", 38);
    const auto       K    = resolvedKey("K", 45);
    const std::array held = {A, K};

    EXPECT_EQ(result->matches({
                  .heldKeys = held,
                  .trigger  = K,
              }),
              BIND_MATCH_FULL);
}

TEST(Keybinds, MultiKeyMatchingIsExactAndOrdered) {
    auto result = makeBind({"A", "K"});
    ASSERT_TRUE(result.has_value());

    const auto       A       = resolvedKey("A", 38);
    const auto       K       = resolvedKey("K", 45);
    const auto       X       = resolvedKey("X", 53);
    const std::array partial = {A};

    EXPECT_EQ(result->matches({
                  .heldKeys = partial,
                  .trigger  = A,
              }),
              BIND_MATCH_PARTIAL);

    const std::array full = {A, K};
    EXPECT_EQ(result->matches({
                  .heldKeys = full,
                  .trigger  = K,
              }),
              BIND_MATCH_FULL);
    EXPECT_EQ(result->matches({
                  .heldKeys = full,
                  .trigger  = A,
              }),
              BIND_MATCH_NONE);

    const std::array extra = {A, K, X};
    EXPECT_EQ(result->matches({
                  .heldKeys = extra,
                  .trigger  = K,
              }),
              BIND_MATCH_PARTIAL);
}

TEST(Keybinds, OneHeldKeyCannotSatisfyTwoPatterns) {
    auto result = makeBind({"A", "code:38"});
    ASSERT_TRUE(result.has_value());

    const auto       A    = resolvedKey("A", 38);
    const std::array held = {A};

    EXPECT_EQ(result->matches({
                  .heldKeys = held,
                  .trigger  = A,
              }),
              BIND_MATCH_PARTIAL);
}

TEST(Keybinds, ReleaseBindCompletesOnRelease) {
    auto result = makeBind({"K"}, BIND_FLAG_RELEASE);
    ASSERT_TRUE(result.has_value());

    const auto       K    = resolvedKey("K", 45);
    const std::array held = {K};

    EXPECT_EQ(result->matches({
                  .heldKeys = held,
                  .trigger  = K,
                  .pressed  = true,
              }),
              BIND_MATCH_PARTIAL);
    EXPECT_EQ(result->matches({
                  .heldKeys = held,
                  .trigger  = K,
                  .pressed  = false,
              }),
              BIND_MATCH_FULL);
}

TEST(Keybinds, ReleaseBindUsesModifiersAtPress) {
    auto result = makeBind({"SUPER", "K"}, BIND_FLAG_RELEASE);
    ASSERT_TRUE(result.has_value());

    const auto       K    = resolvedKey("K", 45);
    const std::array held = {K};

    EXPECT_EQ(result->matches({
                  .heldKeys         = held,
                  .trigger          = K,
                  .modifiersNow     = HL_MODIFIER_NONE,
                  .modifiersAtPress = HL_MODIFIER_META,
                  .pressed          = false,
              }),
              BIND_MATCH_FULL);
}

TEST(Keybinds, ReleaseBindIgnoresModifiersPressedAfterTrigger) {
    auto result = makeBind({"SUPER", "K"}, BIND_FLAG_RELEASE);
    ASSERT_TRUE(result.has_value());

    const auto       SUPER = resolvedKey("SUPER_L", 133, HL_MODIFIER_META);
    const auto       K     = resolvedKey("K", 45);
    const std::array held  = {K, SUPER};

    EXPECT_EQ(result->matches({
                  .heldKeys         = held,
                  .trigger          = K,
                  .modifiersNow     = HL_MODIFIER_META,
                  .modifiersAtPress = HL_MODIFIER_NONE,
                  .pressed          = false,
              }),
              BIND_MATCH_NONE);
}

TEST(Keybinds, ModifierReleaseBindIncludesTriggerModifier) {
    auto result = makeBind({"SUPER", "SUPER_L"}, BIND_FLAG_RELEASE);
    ASSERT_TRUE(result.has_value());

    const auto       SUPER = resolvedKey("SUPER_L", 133, HL_MODIFIER_META);
    const std::array held  = {SUPER};

    EXPECT_EQ(result->matches({
                  .heldKeys         = held,
                  .trigger          = SUPER,
                  .modifiersNow     = HL_MODIFIER_NONE,
                  .modifiersAtPress = HL_MODIFIER_NONE,
                  .pressed          = false,
              }),
              BIND_MATCH_FULL);
}

TEST(Keybinds, RejectsRepeatedKeys) {
    auto result = makeBind({"A", "A", "A"});
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Repeated key"), std::string::npos);

    result = makeBind({"SUPER", "SUPER", "K"});
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Repeated key"), std::string::npos);

    result = makeBind({"a", "A"});
    ASSERT_FALSE(result.has_value());
}

TEST(Keybinds, CatchAllContextHonorsSubmapAndModifiers) {
    auto result = CBind::make({"SUPER", "catchall"}, BIND_FLAG_CATCH_ALL, [] { return SBindResult{}; }, {.metadata = {.submap = "resize"}});
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result->matchesContext({.modifiersNow = HL_MODIFIER_META, .submap = "resize"}));
    EXPECT_FALSE(result->matchesContext({.modifiersNow = HL_MODIFIER_META, .submap = "other"}));
    EXPECT_FALSE(result->matchesContext({.submap = "resize"}));
}
