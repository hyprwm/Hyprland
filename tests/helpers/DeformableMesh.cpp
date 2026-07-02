#include <helpers/DeformableMesh.hpp>

#include <gtest/gtest.h>

#include <algorithm>
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
