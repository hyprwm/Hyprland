#include <keybinds/Resolver.hpp>

#include <gtest/gtest.h>

using namespace Keybinds;

TEST(KeybindsResolver, ModifierFromString) {
    EXPECT_EQ(modifierFromString("SHIFT"), HL_MODIFIER_SHIFT);
    EXPECT_EQ(modifierFromString("CONTROL"), HL_MODIFIER_CTRL);
    EXPECT_EQ(modifierFromString("MOD1"), HL_MODIFIER_ALT);
    EXPECT_EQ(modifierFromString("MOD4"), HL_MODIFIER_META);
    EXPECT_EQ(modifierFromString("shift"), std::nullopt);
    EXPECT_EQ(modifierFromString("UNKNOWN"), std::nullopt);
}

TEST(KeybindsResolver, ModMaskFromString) {
    EXPECT_EQ(modMaskFromString("shift ctrl alt super"), HL_MODIFIER_SHIFT | HL_MODIFIER_CTRL | HL_MODIFIER_ALT | HL_MODIFIER_META);
    EXPECT_EQ(modMaskFromString("CAPS MOD2 MOD3 MOD5"), HL_MODIFIER_CAPS | HL_MODIFIER_MOD2 | HL_MODIFIER_MOD3 | HL_MODIFIER_MOD5);
    EXPECT_EQ(modMaskFromString(""), 0);
}

TEST(KeybindsResolver, ExplicitKeycode) {
    EXPECT_EQ(resolver()->resolveKeycode("42"), 42);
    EXPECT_EQ(resolver()->resolveKeycode("code:7"), 7);
    EXPECT_EQ(resolver()->resolveKeycode("mouse:272"), 272);

    const auto INVALID_MOUSE = resolver()->resolveKeycode("mouse:271");
    ASSERT_FALSE(INVALID_MOUSE.has_value());
    EXPECT_EQ(INVALID_MOUSE.error(), "invalid mouse button");
}
