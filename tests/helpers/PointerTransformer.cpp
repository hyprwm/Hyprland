#include <pointer/PointerTransformer.hpp>

#include <gtest/gtest.h>

using namespace Pointer;

TEST(PointerTransformer, ownsCallback) {
    const CPointerTransformer transformer{[offset = Vector2D{15, -7}](Vector2D position) { return position + offset; }};

    EXPECT_EQ(transformer.transform({10, 20}), Vector2D(25, 13));
}

TEST(PointerTransformer, emptyCallbackReturnsInput) {
    const CPointerTransformer transformer{std::function<Vector2D(Vector2D)>{}};

    EXPECT_EQ(transformer.transform({10, 20}), Vector2D(10, 20));
}
