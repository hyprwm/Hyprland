#include <gtest/gtest.h>

#include <managers/SeatManager.hpp>

class CTestKeyboard : public IKeyboard {
  public:
    virtual bool isVirtual() override {
        return false;
    }

    virtual SP<Aquamarine::IKeyboard> aq() override {
        return nullptr;
    }
};

class CTestKeyboardEventHandler : public IKeyboardEventHandler {
  public:
    struct SEvent {
        uint32_t              keycode = 0;
        wl_keyboard_key_state state   = WL_KEYBOARD_KEY_STATE_RELEASED;
    };

    virtual void onKeyboardKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard>) override {
        m_events.emplace_back(SEvent{
            .keycode = event.keycode,
            .state   = event.state,
        });
    }

    std::vector<SEvent> m_events;
};

static IKeyboard::SKeyEvent keyEvent(uint32_t keycode, wl_keyboard_key_state state) {
    return IKeyboard::SKeyEvent{
        .keycode = keycode,
        .state   = state,
    };
}

TEST(KeyboardEventHandlerStack, RoutesNewPressesToTopHandler) {
    CKeyboardEventHandlerStack stack;
    const auto                 KEYBOARD = makeShared<CTestKeyboard>();
    const auto                 FIRST    = makeShared<CTestKeyboardEventHandler>();
    const auto                 SECOND   = makeShared<CTestKeyboardEventHandler>();

    stack.push(FIRST);
    EXPECT_TRUE(stack.dispatch(keyEvent(10, WL_KEYBOARD_KEY_STATE_PRESSED), KEYBOARD, true));

    stack.push(SECOND);
    EXPECT_TRUE(stack.dispatch(keyEvent(11, WL_KEYBOARD_KEY_STATE_PRESSED), KEYBOARD, true));

    ASSERT_EQ(FIRST->m_events.size(), 1);
    EXPECT_EQ(FIRST->m_events.front().keycode, 10);
    ASSERT_EQ(SECOND->m_events.size(), 1);
    EXPECT_EQ(SECOND->m_events.front().keycode, 11);
}

TEST(KeyboardEventHandlerStack, RoutesReleaseToPressOwner) {
    CKeyboardEventHandlerStack stack;
    const auto                 KEYBOARD = makeShared<CTestKeyboard>();
    const auto                 FIRST    = makeShared<CTestKeyboardEventHandler>();
    const auto                 SECOND   = makeShared<CTestKeyboardEventHandler>();

    stack.push(FIRST);
    ASSERT_TRUE(stack.dispatch(keyEvent(10, WL_KEYBOARD_KEY_STATE_PRESSED), KEYBOARD, true));
    stack.push(SECOND);

    EXPECT_TRUE(stack.dispatch(keyEvent(10, WL_KEYBOARD_KEY_STATE_RELEASED), KEYBOARD, false));
    ASSERT_EQ(FIRST->m_events.size(), 2);
    EXPECT_EQ(FIRST->m_events.back().state, WL_KEYBOARD_KEY_STATE_RELEASED);
    EXPECT_TRUE(SECOND->m_events.empty());
}

TEST(KeyboardEventHandlerStack, RemovingTopRestoresPreviousHandler) {
    CKeyboardEventHandlerStack stack;
    const auto                 KEYBOARD = makeShared<CTestKeyboard>();
    const auto                 FIRST    = makeShared<CTestKeyboardEventHandler>();
    const auto                 SECOND   = makeShared<CTestKeyboardEventHandler>();

    stack.push(FIRST);
    stack.push(SECOND);
    EXPECT_TRUE(stack.remove(SECOND));
    EXPECT_TRUE(stack.dispatch(keyEvent(10, WL_KEYBOARD_KEY_STATE_PRESSED), KEYBOARD, true));

    ASSERT_EQ(FIRST->m_events.size(), 1);
    EXPECT_TRUE(SECOND->m_events.empty());
}

TEST(KeyboardEventHandlerStack, ConsumesOwnedReleaseAfterHandlerExpires) {
    CKeyboardEventHandlerStack stack;
    const auto                 KEYBOARD = makeShared<CTestKeyboard>();
    auto                       HANDLER  = makeShared<CTestKeyboardEventHandler>();

    stack.push(HANDLER);
    ASSERT_TRUE(stack.dispatch(keyEvent(10, WL_KEYBOARD_KEY_STATE_PRESSED), KEYBOARD, true));
    HANDLER.reset();

    EXPECT_TRUE(stack.dispatch(keyEvent(10, WL_KEYBOARD_KEY_STATE_RELEASED), KEYBOARD, true));
    EXPECT_FALSE(stack.dispatch(keyEvent(11, WL_KEYBOARD_KEY_STATE_PRESSED), KEYBOARD, true));
}

TEST(KeyboardEventHandlerStack, DropsOwnershipWithKeyboard) {
    CKeyboardEventHandlerStack stack;
    const auto                 KEYBOARD = makeShared<CTestKeyboard>();
    const auto                 HANDLER  = makeShared<CTestKeyboardEventHandler>();

    stack.push(HANDLER);
    ASSERT_TRUE(stack.dispatch(keyEvent(10, WL_KEYBOARD_KEY_STATE_PRESSED), KEYBOARD, true));
    stack.onKeyboardRemoved(KEYBOARD);

    EXPECT_FALSE(stack.dispatch(keyEvent(10, WL_KEYBOARD_KEY_STATE_RELEASED), KEYBOARD, true));
    EXPECT_EQ(HANDLER->m_events.size(), 1);
}
