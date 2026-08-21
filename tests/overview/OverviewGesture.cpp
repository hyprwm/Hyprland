#include <managers/input/trackpad/gestures/OverviewGesture.hpp>
#include <overview/Overview.hpp>

#include <gtest/gtest.h>

class CTestInteractiveOverview final : public Overview::IOverview, public Overview::IOverviewGesture {
  public:
    virtual void open(PHLMONITOR) override {
        m_open = true;
    }

    virtual void close() override {
        m_open = false;
    }

    virtual bool isOpen() const override {
        return m_open;
    }

    virtual bool beginGesture(PHLMONITOR) override {
        m_beginCount++;
        return m_acceptBegin;
    }

    virtual void updateGesture(float completion) override {
        m_completion = completion;
    }

    virtual void endGesture(bool commit) override {
        m_endCount++;
        m_committed = commit;
    }

    bool  m_open        = false;
    bool  m_acceptBegin = true;
    bool  m_committed   = false;
    float m_completion  = 0.F;
    int   m_beginCount  = 0;
    int   m_endCount    = 0;
};

class CScopedTestOverview {
  public:
    CScopedTestOverview() : m_previous(std::move(Overview::overview())) {
        Overview::overview() = makeUnique<CTestInteractiveOverview>();
    }

    ~CScopedTestOverview() {
        Overview::overview() = std::move(m_previous);
    }

    CTestInteractiveOverview& get() const {
        return *dynamic_cast<CTestInteractiveOverview*>(Overview::overview().get());
    }

  private:
    UP<Overview::IOverview> m_previous;
};

TEST(OverviewGesture, tracksSwipeAndUsesCommitThreshold) {
    CScopedTestOverview         scopedOverview;
    COverviewTrackpadGesture    gesture;
    IPointer::SSwipeUpdateEvent update{.delta = {0, -40}};
    IPointer::SSwipeEndEvent    end;

    gesture.begin({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_UP});
    gesture.update({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_UP});

    EXPECT_EQ(scopedOverview.get().m_beginCount, 1);
    EXPECT_FLOAT_EQ(scopedOverview.get().m_completion, 0.2F);

    gesture.end({.swipe = &end, .direction = TRACKPAD_GESTURE_DIR_UP});
    EXPECT_EQ(scopedOverview.get().m_endCount, 1);
    EXPECT_FALSE(scopedOverview.get().m_committed);

    update.delta = {0, -60};
    gesture.begin({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_UP});
    gesture.update({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_UP});
    gesture.end({.swipe = &end, .direction = TRACKPAD_GESTURE_DIR_UP});

    EXPECT_TRUE(scopedOverview.get().m_committed);
}

TEST(OverviewGesture, reversesAndClampsProgress) {
    CScopedTestOverview         scopedOverview;
    COverviewTrackpadGesture    gesture;
    IPointer::SSwipeUpdateEvent update{.delta = {-250, 0}};

    gesture.begin({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_LEFT});
    gesture.update({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_LEFT});
    EXPECT_FLOAT_EQ(scopedOverview.get().m_completion, 1.F);

    update.delta = {300, 0};
    gesture.update({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_LEFT});
    EXPECT_FLOAT_EQ(scopedOverview.get().m_completion, 0.F);
}

TEST(OverviewGesture, cancellationOverridesCommit) {
    CScopedTestOverview         scopedOverview;
    IPointer::SSwipeUpdateEvent update{.delta = {0, 200}};
    IPointer::SSwipeEndEvent    end{.cancelled = true};

    {
        COverviewTrackpadGesture gesture;
        gesture.begin({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_DOWN});
        gesture.update({.swipe = &update, .direction = TRACKPAD_GESTURE_DIR_DOWN});
        gesture.end({.swipe = &end, .direction = TRACKPAD_GESTURE_DIR_DOWN});
    }

    EXPECT_EQ(scopedOverview.get().m_endCount, 1);
    EXPECT_FALSE(scopedOverview.get().m_committed);
}

TEST(OverviewGesture, supportsScaledPinchDistance) {
    CScopedTestOverview         scopedOverview;
    COverviewTrackpadGesture    gesture;
    IPointer::SPinchUpdateEvent update{.scale = 1.1};

    gesture.begin({.pinch = &update, .direction = TRACKPAD_GESTURE_DIR_PINCH_IN, .scale = 2.F});
    gesture.update({.pinch = &update, .direction = TRACKPAD_GESTURE_DIR_PINCH_IN, .scale = 2.F});

    EXPECT_FLOAT_EQ(scopedOverview.get().m_completion, 0.4F);
}
