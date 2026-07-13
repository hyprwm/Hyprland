#include <event/EventBus.hpp>

#include <gtest/gtest.h>

using namespace Event;

TEST(EventBusCustomEvent, emitWithCorrectArgsSucceedsAndDelivers) {
    CEventBus::CCustomEvent e("test", {CEventBus::CCustomEvent::TYPE_INT, CEventBus::CCustomEvent::TYPE_STRING});

    int                     receivedInt = 0;
    std::string             receivedStr;

    auto                    listener = e.m_event.listen([&](const std::vector<CEventBus::CCustomEvent::ValidVariant>& args) {
        receivedInt = std::get<int>(args[0]);
        receivedStr = std::get<std::string>(args[1]);
    });

    EXPECT_TRUE(e.emit({42, std::string("hello")}));
    EXPECT_EQ(receivedInt, 42);
    EXPECT_EQ(receivedStr, "hello");
}

TEST(EventBusCustomEvent, emitWithTooFewArgsFails) {
    CEventBus::CCustomEvent e("test", {CEventBus::CCustomEvent::TYPE_INT, CEventBus::CCustomEvent::TYPE_STRING});

    const auto              result = e.emit({42});
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Too few"), std::string::npos);
}

TEST(EventBusCustomEvent, emitWithTooManyArgsFails) {
    CEventBus::CCustomEvent e("test", {CEventBus::CCustomEvent::TYPE_INT});

    const auto              result = e.emit({42, std::string("extra")});
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Too many"), std::string::npos);
}

TEST(EventBusCustomEvent, emitWithWrongArgTypeFails) {
    CEventBus::CCustomEvent e("test", {CEventBus::CCustomEvent::TYPE_BOOL});

    const auto              result = e.emit({std::string("not a bool")});
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Invalid type for arg 0"), std::string::npos);
}

TEST(EventBusCustomEvent, listenerReceivesCorrectArgTypes) {
    CEventBus::CCustomEvent e("typed",
                              {CEventBus::CCustomEvent::TYPE_BOOL, CEventBus::CCustomEvent::TYPE_INT, CEventBus::CCustomEvent::TYPE_DOUBLE, CEventBus::CCustomEvent::TYPE_STRING});

    CEventBus::CCustomEvent::ValidVariant receivedBool{false};
    CEventBus::CCustomEvent::ValidVariant receivedInt{0};
    CEventBus::CCustomEvent::ValidVariant receivedDouble{0.0};
    CEventBus::CCustomEvent::ValidVariant receivedString{std::string("")};

    auto                                  listener = e.m_event.listen([&](const std::vector<CEventBus::CCustomEvent::ValidVariant>& args) {
        receivedBool   = args[0];
        receivedInt    = args[1];
        receivedDouble = args[2];
        receivedString = args[3];
    });

    const auto                            result = e.emit({true, 99, 3.14, std::string("hello")});
    EXPECT_TRUE(result.has_value());

    EXPECT_EQ(std::get<bool>(receivedBool), true);
    EXPECT_EQ(std::get<int>(receivedInt), 99);
    EXPECT_DOUBLE_EQ(std::get<double>(receivedDouble), 3.14);
    EXPECT_EQ(std::get<std::string>(receivedString), "hello");
}
