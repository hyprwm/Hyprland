#include <gtest/gtest.h>

#include <pointer/PointerManager.hpp>
#include <pointer/PointerTransformer.hpp>

using namespace Pointer;

TEST(PointerTransformer, AppliesCallback) {
    const CPointerTransformer transformer{[offset = Vector2D{15, -7}](Vector2D position) { return position + offset; }};

    EXPECT_EQ(transformer.transform({10, 20}), Vector2D(25, 13));
}

TEST(PointerTransformer, EmptyCallbackReturnsInput) {
    const CPointerTransformer transformer{std::function<Vector2D(Vector2D)>{}};

    EXPECT_EQ(transformer.transform({10, 20}), Vector2D(10, 20));
}

TEST(PointerTransformerStack, AppliesTransformersInRegistrationOrder) {
    CPointerManager manager;

    const auto      OFFSET = makeShared<CPointerTransformer>([](Vector2D position) { return position + Vector2D{2, 3}; });
    const auto      SCALE  = makeShared<CPointerTransformer>([](Vector2D position) { return position * 2; });

    manager.addTransformer(OFFSET);
    manager.addTransformer(SCALE);

    EXPECT_TRUE(manager.hasTransformers());
    EXPECT_EQ(manager.untransformedPosition(), Vector2D(0, 0));
    EXPECT_EQ(manager.position(), Vector2D(4, 6));
}

TEST(PointerTransformerStack, IgnoresDuplicateAndNullTransformers) {
    CPointerManager manager;
    int             calls       = 0;
    const auto      TRANSFORMER = makeShared<CPointerTransformer>([&calls](Vector2D position) {
        ++calls;
        return position + Vector2D{1, 1};
    });

    manager.addTransformer(nullptr);
    manager.addTransformer(TRANSFORMER);
    manager.addTransformer(TRANSFORMER);

    EXPECT_EQ(manager.position(), Vector2D(1, 1));
    EXPECT_EQ(calls, 1);
}

TEST(PointerTransformerStack, RemovesTransformers) {
    CPointerManager manager;
    const auto      TRANSFORMER = makeShared<CPointerTransformer>([](Vector2D position) { return position + Vector2D{1, 1}; });

    manager.addTransformer(TRANSFORMER);
    manager.removeTransformer(TRANSFORMER);

    EXPECT_FALSE(manager.hasTransformers());
    EXPECT_EQ(manager.position(), manager.untransformedPosition());
}

TEST(PointerTransformerStack, DefersMutationsUntilNextTransform) {
    CPointerManager manager;
    const auto      SECOND = makeShared<CPointerTransformer>([](Vector2D position) { return position + Vector2D{10, 10}; });
    const auto      FIRST  = makeShared<CPointerTransformer>([&manager, SECOND](Vector2D position) {
        manager.removeTransformer(SECOND);
        return position + Vector2D{1, 1};
    });

    manager.addTransformer(FIRST);
    manager.addTransformer(SECOND);

    EXPECT_EQ(manager.position(), Vector2D(11, 11));
    EXPECT_EQ(manager.position(), Vector2D(1, 1));
}
