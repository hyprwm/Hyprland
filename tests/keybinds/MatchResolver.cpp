#include <keybinds/MatchResolver.hpp>

#include <gtest/gtest.h>

#include <array>

using namespace Keybinds;

static PBind makeResolverBind(std::vector<std::string>&& keys, BindFlags flags = 0) {
    auto bind = CBind::make(std::move(keys), flags, [] { return SBindResult{}; });
    EXPECT_TRUE(bind.has_value());
    return makeShared<CBind>(std::move(*bind));
}

static SResolvedKey resolverKey(const char* name, xkb_keycode_t code) {
    return {
        .sym  = xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE),
        .code = code,
    };
}

TEST(KeybindsMatchResolver, LongerFullChordSuppressesShorterFullChord) {
    const auto              SHORT = makeResolverBind({"SUPER", "K"});
    const auto              LONG  = makeResolverBind({"SUPER", "Q", "K"});
    const auto              Q     = resolverKey("Q", 24);
    const auto              K     = resolverKey("K", 45);
    const std::array        HELD  = {Q, K};
    const SBindEventContext CONTEXT{
        .heldKeys     = HELD,
        .trigger      = K,
        .modifiersNow = HL_MODIFIER_META,
    };
    const std::array CANDIDATES = {
        SBindMatchCandidate{SHORT, SHORT->matches(CONTEXT)},
        SBindMatchCandidate{LONG, LONG->matches(CONTEXT)},
    };

    const auto RESOLUTION = resolveChordMatches(CANDIDATES, CONTEXT);
    ASSERT_EQ(RESOLUTION.immediate.size(), 1);
    EXPECT_EQ(RESOLUTION.immediate.front(), LONG);
    EXPECT_TRUE(RESOLUTION.deferred.empty());
}

TEST(KeybindsMatchResolver, FullPrefixWaitsForLongerChord) {
    const auto              SHORT = makeResolverBind({"SUPER", "Q"});
    const auto              LONG  = makeResolverBind({"SUPER", "Q", "K"});
    const auto              Q     = resolverKey("Q", 24);
    const std::array        HELD  = {Q};
    const SBindEventContext CONTEXT{
        .heldKeys     = HELD,
        .trigger      = Q,
        .modifiersNow = HL_MODIFIER_META,
    };
    const std::array CANDIDATES = {
        SBindMatchCandidate{SHORT, SHORT->matches(CONTEXT)},
        SBindMatchCandidate{LONG, LONG->matches(CONTEXT)},
    };

    const auto RESOLUTION = resolveChordMatches(CANDIDATES, CONTEXT);
    EXPECT_TRUE(RESOLUTION.immediate.empty());
    ASSERT_EQ(RESOLUTION.deferred.size(), 1);
    EXPECT_EQ(RESOLUTION.deferred.front(), SHORT);
}

TEST(KeybindsMatchResolver, ReverseOrderDoesNotDeferShortChord) {
    const auto              SHORT = makeResolverBind({"SUPER", "Q"});
    const auto              LONG  = makeResolverBind({"SUPER", "K", "Q"});
    const auto              Q     = resolverKey("Q", 24);
    const std::array        HELD  = {Q};
    const SBindEventContext CONTEXT{
        .heldKeys     = HELD,
        .trigger      = Q,
        .modifiersNow = HL_MODIFIER_META,
    };
    const std::array CANDIDATES = {
        SBindMatchCandidate{SHORT, SHORT->matches(CONTEXT)},
        SBindMatchCandidate{LONG, LONG->matches(CONTEXT)},
    };

    const auto RESOLUTION = resolveChordMatches(CANDIDATES, CONTEXT);
    ASSERT_EQ(RESOLUTION.immediate.size(), 1);
    EXPECT_EQ(RESOLUTION.immediate.front(), SHORT);
    EXPECT_TRUE(RESOLUTION.deferred.empty());
}

TEST(KeybindsMatchResolver, SidedModifierDoesNotIncreaseChordLength) {
    const auto         SHORT = makeResolverBind({"SHIFT_L", "Q"});
    const auto         LONG  = makeResolverBind({"SHIFT", "Q", "K"});
    const SResolvedKey SHIFT{
        .sym      = XKB_KEY_Shift_L,
        .code     = 50,
        .modifier = HL_MODIFIER_SHIFT,
    };
    const auto              Q    = resolverKey("Q", 24);
    const std::array        HELD = {SHIFT, Q};
    const SBindEventContext CONTEXT{
        .heldKeys     = HELD,
        .trigger      = Q,
        .modifiersNow = HL_MODIFIER_SHIFT,
    };
    const std::array CANDIDATES = {
        SBindMatchCandidate{SHORT, SHORT->matches(CONTEXT)},
        SBindMatchCandidate{LONG, LONG->matches(CONTEXT)},
    };

    const auto RESOLUTION = resolveChordMatches(CANDIDATES, CONTEXT);
    EXPECT_EQ(SHORT->chordSize(), 1);
    EXPECT_EQ(LONG->chordSize(), 2);
    EXPECT_TRUE(RESOLUTION.immediate.empty());
    ASSERT_EQ(RESOLUTION.deferred.size(), 1);
    EXPECT_EQ(RESOLUTION.deferred.front(), SHORT);
}

TEST(KeybindsMatchResolver, SidedModifierSubChordIsSuppressed) {
    const auto         SHORT = makeResolverBind({"SHIFT_L", "K"});
    const auto         LONG  = makeResolverBind({"SHIFT", "Q", "K"});
    const SResolvedKey SHIFT{
        .sym      = XKB_KEY_Shift_L,
        .code     = 50,
        .modifier = HL_MODIFIER_SHIFT,
    };
    const auto              Q    = resolverKey("Q", 24);
    const auto              K    = resolverKey("K", 45);
    const std::array        HELD = {SHIFT, Q, K};
    const SBindEventContext CONTEXT{
        .heldKeys     = HELD,
        .trigger      = K,
        .modifiersNow = HL_MODIFIER_SHIFT,
    };

    EXPECT_TRUE(SHORT->isSubChordOf(*LONG, CONTEXT));
}

TEST(KeybindsMatchResolver, LongPressPrefixIsDeferred) {
    const auto              SHORT = makeResolverBind({"SUPER", "Q"}, BIND_FLAG_LONG_PRESS);
    const auto              LONG  = makeResolverBind({"SUPER", "Q", "K"});
    const auto              Q     = resolverKey("Q", 24);
    const std::array        HELD  = {Q};
    const SBindEventContext CONTEXT{
        .heldKeys     = HELD,
        .trigger      = Q,
        .modifiersNow = HL_MODIFIER_META,
    };
    const std::array CANDIDATES = {
        SBindMatchCandidate{SHORT, SHORT->matches(CONTEXT)},
        SBindMatchCandidate{LONG, LONG->matches(CONTEXT)},
    };

    const auto RESOLUTION = resolveChordMatches(CANDIDATES, CONTEXT);
    EXPECT_TRUE(RESOLUTION.immediate.empty());
    ASSERT_EQ(RESOLUTION.deferred.size(), 1);
    EXPECT_EQ(RESOLUTION.deferred.front(), SHORT);
}

TEST(KeybindsMatchResolver, ReleaseChordArmsOnlyWhenFullyHeld) {
    const auto              BIND        = makeResolverBind({"SUPER", "Q", "K"}, BIND_FLAG_RELEASE);
    const auto              Q           = resolverKey("Q", 24);
    const auto              K           = resolverKey("K", 45);
    const std::array        K_ONLY_HELD = {K};
    const SBindEventContext INCOMPLETE{
        .heldKeys     = K_ONLY_HELD,
        .trigger      = K,
        .modifiersNow = HL_MODIFIER_META,
    };
    const std::array        FULLY_HELD = {Q, K};
    const SBindEventContext COMPLETE{
        .heldKeys     = FULLY_HELD,
        .trigger      = K,
        .modifiersNow = HL_MODIFIER_META,
    };

    EXPECT_FALSE(BIND->isFullyHeld(INCOMPLETE));
    EXPECT_TRUE(BIND->isFullyHeld(COMPLETE));
}
