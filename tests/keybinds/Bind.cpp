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

TEST(Keybinds, SidedModifierAfterOrdinaryChordKeysDoesNotPartiallyMatch) {
    auto result = makeBind({"SHIFT_L", "A", "B"});
    ASSERT_TRUE(result.has_value());

    const auto       SHIFT = resolvedKey("SHIFT_L", 50, HL_MODIFIER_SHIFT);
    const auto       A     = resolvedKey("A", 38);
    const auto       B     = resolvedKey("B", 56);
    const std::array held  = {A, B, SHIFT};

    EXPECT_EQ(result->matches({.heldKeys = held, .trigger = SHIFT}), BIND_MATCH_NONE);
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

TEST(Keybinds, ChordPartialMatchRequiresOrderedPrefixTrigger) {
    auto result = makeBind({"TAB", "T"});
    ASSERT_TRUE(result.has_value());

    const auto       TAB = resolvedKey("TAB", 23);
    const auto       T   = resolvedKey("T", 28);
    const auto       X   = resolvedKey("X", 53);
    const std::array tab = {TAB};
    const std::array t   = {T};

    EXPECT_EQ(result->matches({.heldKeys = tab, .trigger = TAB}), BIND_MATCH_PARTIAL);
    EXPECT_EQ(result->matches({.heldKeys = t, .trigger = T}), BIND_MATCH_NONE);

    const std::array unrelated = {TAB, X};
    EXPECT_EQ(result->matches({.heldKeys = unrelated, .trigger = X}), BIND_MATCH_NONE);

    const std::array reversed = {T, TAB};
    EXPECT_EQ(result->matches({.heldKeys = reversed, .trigger = TAB}), BIND_MATCH_PARTIAL);
    EXPECT_FALSE(result->isFullyHeld({.heldKeys = reversed, .trigger = T}));

    const std::array full = {TAB, T};
    EXPECT_EQ(result->matches({.heldKeys = full, .trigger = T}), BIND_MATCH_FULL);
}

TEST(Keybinds, ThreeKeyChordRequiresPressOrder) {
    const auto       A   = resolvedKey("A", 38);
    const auto       B   = resolvedKey("B", 56);
    const auto       C   = resolvedKey("C", 54);
    const std::array a   = {A};
    const std::array ab  = {A, B};
    const std::array ac  = {A, C};
    const std::array abc = {A, B, C};
    const std::array bac = {B, A, C};
    const std::array acb = {A, C, B};

    for (const auto flags : {sc<eBindFlags>(0), BIND_FLAG_NON_CONSUMING, BIND_FLAG_RELEASE}) {
        auto result = makeBind({"A", "B", "C"}, flags);
        ASSERT_TRUE(result.has_value());

        EXPECT_EQ(result->matches({.heldKeys = a, .trigger = A}), BIND_MATCH_PARTIAL);
        EXPECT_EQ(result->matches({.heldKeys = ab, .trigger = B}), BIND_MATCH_PARTIAL);
        EXPECT_EQ(result->matches({.heldKeys = ac, .trigger = C}), BIND_MATCH_NONE);
        EXPECT_EQ(result->matches({.heldKeys = abc, .trigger = C}), flags == BIND_FLAG_RELEASE ? BIND_MATCH_PARTIAL : BIND_MATCH_FULL);
        EXPECT_EQ(result->matches({.heldKeys = bac, .trigger = C}), BIND_MATCH_NONE);
        EXPECT_EQ(result->matches({.heldKeys = acb, .trigger = B}), BIND_MATCH_PARTIAL);
        EXPECT_TRUE(result->isFullyHeld({.heldKeys = abc, .trigger = C}));
        EXPECT_FALSE(result->isFullyHeld({.heldKeys = bac, .trigger = C}));
        EXPECT_FALSE(result->isFullyHeld({.heldKeys = acb, .trigger = C}));
    }
}

TEST(Keybinds, OrderedChordSupportsKeycodePatterns) {
    auto result = makeBind({"A", "code:38"});
    ASSERT_TRUE(result.has_value());

    const auto       FIRST    = resolvedKey("A", 39);
    const auto       SECOND   = resolvedKey("A", 38);
    const std::array full     = {FIRST, SECOND};
    const std::array reversed = {SECOND, FIRST};

    EXPECT_EQ(result->matches({.heldKeys = full, .trigger = SECOND}), BIND_MATCH_FULL);
    EXPECT_EQ(result->matches({.heldKeys = reversed, .trigger = FIRST}), BIND_MATCH_NONE);
}

TEST(Keybinds, ReleaseChordStillMatchesAfterPrefixIsReleased) {
    auto result = makeBind({"A", "B", "C"}, BIND_FLAG_RELEASE);
    ASSERT_TRUE(result.has_value());

    const auto       C    = resolvedKey("C", 54);
    const std::array held = {C};

    EXPECT_EQ(result->matches({.heldKeys = held, .trigger = C}), BIND_MATCH_NONE);
    EXPECT_FALSE(result->isFullyHeld({.heldKeys = held, .trigger = C}));
    // The manager checks that the release chord was armed on an ordered full press.
    EXPECT_EQ(result->matches({.heldKeys = held, .trigger = C, .pressed = false}), BIND_MATCH_FULL);
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

TEST(Keybinds, CatchAllContextHonorsSubmapAndModifiers) {
    auto result = CBind::make({"SUPER", "catchall"}, BIND_FLAG_CATCH_ALL, [] { return SBindResult{}; }, {.metadata = {.submap = "resize"}});
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result->matchesContext({.modifiersNow = HL_MODIFIER_META, .submap = "resize"}));
    EXPECT_FALSE(result->matchesContext({.modifiersNow = HL_MODIFIER_META, .submap = "other"}));
    EXPECT_FALSE(result->matchesContext({.submap = "resize"}));
}
