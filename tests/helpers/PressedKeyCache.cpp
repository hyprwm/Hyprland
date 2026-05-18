#include <helpers/PressedKeyCache.hpp>

#include <gtest/gtest.h>

using namespace NInputUtils;

namespace {
    class CFakeKeyboard final : public IKeyboard {
      public:
        explicit CFakeKeyboard(bool isVirtual = false) : m_isVirtual(isVirtual) {}

        bool isVirtual() override {
            return m_isVirtual;
        }

        SP<Aquamarine::IKeyboard> aq() override {
            return nullptr;
        }

        void press(uint32_t key) {
            updatePressed(key, true);
        }

        void setShareStates(bool share) {
            m_shareStatesAuto = false;
            m_shareStates     = share;
        }

        void setEnabled(bool enabled) {
            m_enabled = enabled;
        }

        void setAllowed(bool allowed) {
            m_allowed = allowed;
        }

      private:
        bool m_isVirtual = false;
    };
} // namespace

TEST(Helpers, deduplicatesAcrossKeySets) {
    const auto rebuiltPressedKeys = collectUniquePressedKeys({
        {164, 36},
        {36, 50},
    });

    ASSERT_EQ(rebuiltPressedKeys.size(), 3U);
    EXPECT_EQ(rebuiltPressedKeys[0], 164U);
    EXPECT_EQ(rebuiltPressedKeys[1], 36U);
    EXPECT_EQ(rebuiltPressedKeys[2], 50U);
}

TEST(Helpers, skipsEmptyKeySets) {
    const auto rebuiltPressedKeys = collectUniquePressedKeys({
        {},
        {164},
    });

    ASSERT_EQ(rebuiltPressedKeys.size(), 1U);
    EXPECT_EQ(rebuiltPressedKeys[0], 164U);
}

TEST(Helpers, returnsEmptyForEmptyInput) {
    const auto rebuiltPressedKeys = collectUniquePressedKeys({});

    EXPECT_TRUE(rebuiltPressedKeys.empty());
}

TEST(Helpers, preservesFirstSeenOrder) {
    const auto rebuiltPressedKeys = collectUniquePressedKeys({
        {50, 36},
        {164, 36, 24},
    });

    ASSERT_EQ(rebuiltPressedKeys.size(), 4U);
    EXPECT_EQ(rebuiltPressedKeys[0], 50U);
    EXPECT_EQ(rebuiltPressedKeys[1], 36U);
    EXPECT_EQ(rebuiltPressedKeys[2], 164U);
    EXPECT_EQ(rebuiltPressedKeys[3], 24U);
}

TEST(Helpers, onlyCollectsEligibleKeyboards) {
    auto active = makeShared<CFakeKeyboard>();
    active->press(164);
    active->press(36);

    auto disabled = makeShared<CFakeKeyboard>();
    disabled->press(50);
    disabled->setEnabled(false);

    auto disallowed = makeShared<CFakeKeyboard>();
    disallowed->press(24);
    disallowed->setAllowed(false);

    auto sharedOff = makeShared<CFakeKeyboard>();
    sharedOff->press(57);
    sharedOff->setShareStates(false);

    const auto rebuiltPressedKeys = collectPressedKeysFromKeyboards({active, disabled, disallowed, sharedOff});

    ASSERT_EQ(rebuiltPressedKeys.size(), 2U);
    EXPECT_EQ(rebuiltPressedKeys[0], 164U);
    EXPECT_EQ(rebuiltPressedKeys[1], 36U);
}

TEST(Helpers, keyboardEligibilityMatchesSeatCacheRules) {
    auto keyboard = makeShared<CFakeKeyboard>();

    EXPECT_TRUE(isKeyboardEligibleForPressedKeyCache(*keyboard));

    keyboard->setEnabled(false);
    EXPECT_FALSE(isKeyboardEligibleForPressedKeyCache(*keyboard));

    keyboard->setEnabled(true);
    keyboard->setAllowed(false);
    EXPECT_FALSE(isKeyboardEligibleForPressedKeyCache(*keyboard));

    keyboard->setAllowed(true);
    keyboard->setShareStates(false);
    EXPECT_FALSE(isKeyboardEligibleForPressedKeyCache(*keyboard));
}
