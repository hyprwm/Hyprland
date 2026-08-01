#include <helpers/DeformableMesh.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>

TEST(Helpers, deformableMeshSetSizeClamps) {
    CDeformableMesh mesh;

    mesh.setSize(1);
    EXPECT_EQ(mesh.size(), 2u);

    mesh.setSize(64);
    EXPECT_EQ(mesh.size(), 32u);
}

TEST(Helpers, deformableMeshNoopUpdateKeepsExtents) {
    CDeformableMesh mesh(4);
    CBox            box = {10, 20, 100, 50};

    mesh.onPositionUpdate(box, box, 1.F);

    const auto EXTENTS = mesh.transformedExtents(box);
    EXPECT_DOUBLE_EQ(EXTENTS.x, box.x);
    EXPECT_DOUBLE_EQ(EXTENTS.y, box.y);
    EXPECT_DOUBLE_EQ(EXTENTS.w, box.w);
    EXPECT_DOUBLE_EQ(EXTENTS.h, box.h);
    EXPECT_TRUE(mesh.stable(0.F, 0.F));
}

TEST(Helpers, deformableMeshPositionUpdateExpandsExtents) {
    CDeformableMesh mesh(4);

    const CBox      PREVIOUS = {0, 0, 100, 50};
    const CBox      CURRENT  = {10, 0, 100, 50};
    mesh.onPositionUpdate(PREVIOUS, CURRENT, 1.F);

    const auto EXTENTS = mesh.transformedExtents(CURRENT);
    EXPECT_LT(EXTENTS.x, CURRENT.x);
    EXPECT_GE(EXTENTS.x + EXTENTS.w, CURRENT.x + CURRENT.w);
    EXPECT_FALSE(mesh.stable(0.F, 0.F));
}

TEST(Helpers, deformableMeshGrabPointAnchorsNearestVertices) {
    CDeformableMesh mesh(3);

    const CBox      PREVIOUS = {0, 0, 100, 100};
    const CBox      CURRENT  = {0, 0, 100, 120};
    mesh.onPositionUpdate(PREVIOUS, CURRENT, 1.F, Vector2D{0.5, 0.0});

    const auto VERTICES = mesh.verticesForBox(CURRENT, CURRENT, {100, 120});

    const auto TOPCENTER = std::ranges::find_if(VERTICES, [](const auto& v) { return v.u == 0.5F && v.v == 0.F; });
    const auto CENTER    = std::ranges::find_if(VERTICES, [](const auto& v) { return v.u == 0.5F && v.v == 0.5F; });

    ASSERT_NE(TOPCENTER, VERTICES.end());
    ASSERT_NE(CENTER, VERTICES.end());
    EXPECT_LT(std::abs(TOPCENTER->y - TOPCENTER->v), std::abs(CENTER->y - CENTER->v));
}

TEST(Helpers, deformableMeshPositionUpdateClampsDisplacement) {
    CDeformableMesh mesh(4);

    const CBox      PREVIOUS = {0, 0, 100, 50};
    const CBox      CURRENT  = {1000, 1000, 100, 50};
    mesh.onPositionUpdate(PREVIOUS, CURRENT, 2.F);

    const auto EXTENTS = mesh.transformedExtents(CURRENT);
    EXPECT_GE(EXTENTS.x, CURRENT.x - 16.001);
    EXPECT_GE(EXTENTS.y, CURRENT.y - 16.001);
    EXPECT_LE(EXTENTS.x + EXTENTS.w, CURRENT.x + CURRENT.w + 16.001);
    EXPECT_LE(EXTENTS.y + EXTENTS.h, CURRENT.y + CURRENT.h + 16.001);
}

TEST(Helpers, deformableMeshVerticesForBoxBuildsTriangles) {
    CDeformableMesh mesh(2);

    const auto      VERTICES = mesh.verticesForBox({0, 0, 100, 50}, {0, 0, 100, 50}, {100, 50});

    ASSERT_EQ(VERTICES.size(), 6u);
    EXPECT_FLOAT_EQ(VERTICES.front().x, 0.F);
    EXPECT_FLOAT_EQ(VERTICES.front().y, 0.F);
    EXPECT_FLOAT_EQ(VERTICES.front().u, 0.F);
    EXPECT_FLOAT_EQ(VERTICES.front().v, 0.F);

    EXPECT_FLOAT_EQ(VERTICES.back().x, 1.F);
    EXPECT_FLOAT_EQ(VERTICES.back().y, 1.F);
    EXPECT_FLOAT_EQ(VERTICES.back().u, 1.F);
    EXPECT_FLOAT_EQ(VERTICES.back().v, 1.F);
}

TEST(Helpers, deformableMeshVerticesForBoxTransformsUVs) {
    struct STransformCase {
        eTransform transform = HYPRUTILS_TRANSFORM_NORMAL;
        Vector2D   topLeft;
        Vector2D   bottomRight;
    };

    const CBox         BOX         = {100, 200, 300, 400};
    constexpr Vector2D TEXTURESIZE = {1920, 1080};
    const auto         CASES       = std::array{
        STransformCase{HYPRUTILS_TRANSFORM_NORMAL, {100, 200}, {400, 600}},      STransformCase{HYPRUTILS_TRANSFORM_90, {200, 980}, {600, 680}},
        STransformCase{HYPRUTILS_TRANSFORM_180, {1820, 880}, {1520, 480}},       STransformCase{HYPRUTILS_TRANSFORM_270, {1720, 100}, {1320, 400}},
        STransformCase{HYPRUTILS_TRANSFORM_FLIPPED, {1820, 200}, {1520, 600}},   STransformCase{HYPRUTILS_TRANSFORM_FLIPPED_90, {200, 100}, {600, 400}},
        STransformCase{HYPRUTILS_TRANSFORM_FLIPPED_180, {100, 880}, {400, 480}}, STransformCase{HYPRUTILS_TRANSFORM_FLIPPED_270, {1720, 980}, {1320, 680}},
    };

    CDeformableMesh mesh(2);
    for (const auto& test : CASES) {
        SCOPED_TRACE(static_cast<int>(test.transform));

        const auto VERTICES = mesh.verticesForBox(BOX, BOX, TEXTURESIZE, 1.0, test.transform);

        ASSERT_EQ(VERTICES.size(), 6u);
        EXPECT_FLOAT_EQ(VERTICES.front().u, static_cast<float>(test.topLeft.x / TEXTURESIZE.x));
        EXPECT_FLOAT_EQ(VERTICES.front().v, static_cast<float>(test.topLeft.y / TEXTURESIZE.y));
        EXPECT_FLOAT_EQ(VERTICES.back().u, static_cast<float>(test.bottomRight.x / TEXTURESIZE.x));
        EXPECT_FLOAT_EQ(VERTICES.back().v, static_cast<float>(test.bottomRight.y / TEXTURESIZE.y));
    }
}
