#include <render/OpenGL.hpp>

#include <gtest/gtest.h>

TEST(Render, blurUVUsesDestinationBoxByDefault) {
    const auto UV = Render::GL::resolveBlurUV({100, 200, 300, 400}, {1920, 1080});

    EXPECT_DOUBLE_EQ(UV.x, 100.0 / 1920.0);
    EXPECT_DOUBLE_EQ(UV.y, 200.0 / 1080.0);
    EXPECT_DOUBLE_EQ(UV.w, 300.0 / 1920.0);
    EXPECT_DOUBLE_EQ(UV.h, 400.0 / 1080.0);
}

TEST(Render, blurUVUsesNormalSceneFramebuffer) {
    const CBox SCENE = {0, 0, 1080, 1920};
    const auto UV    = Render::GL::resolveBlurUV(SCENE, SCENE.size());

    EXPECT_DOUBLE_EQ(UV.x, 0.0);
    EXPECT_DOUBLE_EQ(UV.y, 0.0);
    EXPECT_DOUBLE_EQ(UV.w, 1.0);
    EXPECT_DOUBLE_EQ(UV.h, 1.0);
}

TEST(Render, blurUVRejectsInvalidFramebufferSize) {
    EXPECT_TRUE(Render::GL::resolveBlurUV({10, 20, 30, 40}, {}).empty());
}
