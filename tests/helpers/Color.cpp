#include <helpers/Color.hpp>

#include <gtest/gtest.h>

TEST(Helpers, colorConstructorDefault) {
    CHyprColor c;
    EXPECT_DOUBLE_EQ(c.r, 0.0);
    EXPECT_DOUBLE_EQ(c.g, 0.0);
    EXPECT_DOUBLE_EQ(c.b, 0.0);
    EXPECT_DOUBLE_EQ(c.a, 0.0);
}

TEST(Helpers, colorConstructorFloats) {
    CHyprColor c(0.5f, 0.25f, 0.75f, 1.0f);
    EXPECT_FLOAT_EQ(c.r, 0.5);
    EXPECT_FLOAT_EQ(c.g, 0.25);
    EXPECT_FLOAT_EQ(c.b, 0.75);
    EXPECT_FLOAT_EQ(c.a, 1.0);
}

TEST(Helpers, colorConstructorHex) {
    // Format: 0xAARRGGBB
    CHyprColor white(0xFFFFFFFFULL);
    EXPECT_NEAR(white.r, 1.0, 0.01);
    EXPECT_NEAR(white.g, 1.0, 0.01);
    EXPECT_NEAR(white.b, 1.0, 0.01);
    EXPECT_NEAR(white.a, 1.0, 0.01);

    CHyprColor red(0xFFFF0000ULL);
    EXPECT_NEAR(red.r, 1.0, 0.01);
    EXPECT_NEAR(red.g, 0.0, 0.01);
    EXPECT_NEAR(red.b, 0.0, 0.01);
    EXPECT_NEAR(red.a, 1.0, 0.01);

    CHyprColor transparent(0x00000000ULL);
    EXPECT_NEAR(transparent.r, 0.0, 0.01);
    EXPECT_NEAR(transparent.g, 0.0, 0.01);
    EXPECT_NEAR(transparent.b, 0.0, 0.01);
    EXPECT_NEAR(transparent.a, 0.0, 0.01);
}

TEST(Helpers, colorGetAsHex) {
    CHyprColor white(1.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_EQ(white.getAsHex(), 0xFFFFFFFF);

    CHyprColor black(0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_EQ(black.getAsHex(), 0xFF000000);

    CHyprColor transparent(0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(transparent.getAsHex(), 0x00000000);
}

TEST(Helpers, colorStripA) {
    CHyprColor c(0.5f, 0.25f, 0.75f, 0.3f);
    CHyprColor stripped = c.stripA();

    EXPECT_FLOAT_EQ(stripped.r, 0.5);
    EXPECT_FLOAT_EQ(stripped.g, 0.25);
    EXPECT_FLOAT_EQ(stripped.b, 0.75);
    EXPECT_FLOAT_EQ(stripped.a, 1.0);

    // original unchanged
    EXPECT_FLOAT_EQ(c.a, 0.3f);
}

TEST(Helpers, colorModifyA) {
    CHyprColor c(0.5f, 0.25f, 0.75f, 1.0f);
    CHyprColor modified = c.modifyA(0.5f);

    EXPECT_FLOAT_EQ(modified.r, 0.5);
    EXPECT_FLOAT_EQ(modified.g, 0.25);
    EXPECT_FLOAT_EQ(modified.b, 0.75);
    EXPECT_FLOAT_EQ(modified.a, 0.5);

    // original unchanged
    EXPECT_FLOAT_EQ(c.a, 1.0);
}

TEST(Helpers, colorEquality) {
    CHyprColor a(1.0f, 0.0f, 0.0f, 1.0f);
    CHyprColor b(1.0f, 0.0f, 0.0f, 1.0f);
    CHyprColor c(0.0f, 1.0f, 0.0f, 1.0f);
    CHyprColor d(1.0f, 0.0f, 0.0f, 0.5f);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d); // different alpha
}

TEST(Helpers, colorConstants) {
    EXPECT_EQ(Colors::WHITE, CHyprColor(1.0f, 1.0f, 1.0f, 1.0f));
    EXPECT_EQ(Colors::BLACK, CHyprColor(0.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(Colors::RED, CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(Colors::GREEN, CHyprColor(0.0f, 1.0f, 0.0f, 1.0f));
    EXPECT_EQ(Colors::BLUE, CHyprColor(0.0f, 0.0f, 1.0f, 1.0f));
}
