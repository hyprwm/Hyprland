#include <desktop/view/focusable/Focusable.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;

class CTestFocusable : public IFocusable {
  public:
    virtual bool focusAvailable() const override {
        return m_available;
    }

    void setAvailable(bool available) {
        m_available = available;
    }

    int stateUpdates() const {
        return m_stateUpdates;
    }

    bool lastBlockedState() const {
        return m_lastBlockedState;
    }

  protected:
    virtual void onInputBlockStateUpdated(bool blocked) override {
        m_stateUpdates++;
        m_lastBlockedState = blocked;
    }

  private:
    bool m_available        = true;
    bool m_lastBlockedState = false;
    int  m_stateUpdates     = 0;
};

TEST(Focusable, CombinesAvailabilityAndBlockState) {
    CTestFocusable focusable;

    EXPECT_TRUE(focusable.acceptsInput());

    focusable.setAvailable(false);
    EXPECT_FALSE(focusable.acceptsInput());

    focusable.setAvailable(true);
    focusable.setInputBlocked(FOCUS_BLOCK_GROUP_INACTIVE, true);
    EXPECT_FALSE(focusable.acceptsInput());
    EXPECT_TRUE(focusable.lastBlockedState());
    EXPECT_EQ(focusable.stateUpdates(), 1);

    focusable.setInputBlocked(FOCUS_BLOCK_GROUP_INACTIVE, false);
    EXPECT_TRUE(focusable.acceptsInput());
    EXPECT_FALSE(focusable.lastBlockedState());
    EXPECT_EQ(focusable.stateUpdates(), 2);
}

TEST(Focusable, TracksIndependentBlockReasons) {
    CTestFocusable focusable;

    focusable.setInputBlocked(FOCUS_BLOCK_GROUP_INACTIVE, true);
    focusable.setInputBlocked(FOCUS_BLOCK_BELOW_FULLSCREEN, true);

    EXPECT_TRUE(focusable.isInputBlocked());
    EXPECT_TRUE(focusable.isInputBlockedReasonAnyOf(FOCUS_BLOCK_GROUP_INACTIVE));
    EXPECT_TRUE(focusable.isInputBlockedReasonAnyOf(FOCUS_BLOCK_MONOCLE_INACTIVE | FOCUS_BLOCK_BELOW_FULLSCREEN));
    EXPECT_TRUE(focusable.hasInputBlockedReasonsBesides(FOCUS_BLOCK_GROUP_INACTIVE));
    EXPECT_FALSE(focusable.noInputBlockedReasonsBesides(FOCUS_BLOCK_GROUP_INACTIVE));
    EXPECT_EQ(focusable.stateUpdates(), 2);

    focusable.setInputBlocked(FOCUS_BLOCK_GROUP_INACTIVE, false);

    EXPECT_TRUE(focusable.isInputBlocked());
    EXPECT_TRUE(focusable.noInputBlockedReasonsBesides(FOCUS_BLOCK_BELOW_FULLSCREEN));
    EXPECT_EQ(focusable.stateUpdates(), 3);

    focusable.setInputBlocked(FOCUS_BLOCK_BELOW_FULLSCREEN, false);

    EXPECT_FALSE(focusable.isInputBlocked());
    EXPECT_TRUE(focusable.noInputBlockedReasonsBesides(FOCUS_BLOCK_NONE));
    EXPECT_EQ(focusable.stateUpdates(), 4);
}

TEST(Focusable, IgnoresNoneReason) {
    CTestFocusable focusable;

    focusable.setInputBlocked(FOCUS_BLOCK_NONE, true);

    EXPECT_FALSE(focusable.isInputBlocked());
    EXPECT_EQ(focusable.stateUpdates(), 0);
}
