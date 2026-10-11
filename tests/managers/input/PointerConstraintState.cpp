#include <managers/input/PointerConstraintState.hpp>
#include <gtest/gtest.h>

TEST(PointerConstraintState, PersistentReactivation) {
    CPointerConstraintState state;

    EXPECT_FALSE(state.active());
    EXPECT_TRUE(state.activate());
    EXPECT_TRUE(state.active());
    EXPECT_TRUE(state.deactivate());
    EXPECT_FALSE(state.active());
    EXPECT_FALSE(state.exhausted());
    EXPECT_TRUE(state.activate());
    EXPECT_TRUE(state.active());
}

TEST(PointerConstraintState, OneshotCannotReactivate) {
    CPointerConstraintState state(CPointerConstraintState::LIFETIME_ONESHOT);

    EXPECT_TRUE(state.activate());
    EXPECT_TRUE(state.deactivate());
    EXPECT_TRUE(state.exhausted());
    EXPECT_FALSE(state.activate());
    EXPECT_FALSE(state.active());
}

TEST(PointerConstraintState, InactiveOneshotIsNotConsumed) {
    CPointerConstraintState state(CPointerConstraintState::LIFETIME_ONESHOT);

    EXPECT_FALSE(state.deactivate());
    EXPECT_FALSE(state.exhausted());
    EXPECT_TRUE(state.activate());
}

TEST(PointerConstraintState, RepeatedTransitionsDoNotNotify) {
    CPointerConstraintState state;

    EXPECT_TRUE(state.activate());
    EXPECT_FALSE(state.activate());
    EXPECT_TRUE(state.active());
    EXPECT_TRUE(state.deactivate());
    EXPECT_FALSE(state.deactivate());
}
