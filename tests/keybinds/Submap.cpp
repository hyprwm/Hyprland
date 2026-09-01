#include <devices/Keyboard.hpp>
#include <helpers/memory/Memory.hpp>
#include <keybinds/Submap.hpp>

#include <aquamarine/input/Input.hpp>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "Utils.hpp"

using namespace Keybinds;

namespace {
    class CTestKeyboard : public Aquamarine::IKeyboard {
      public:
        static SP<CTestKeyboard> make(std::string&& name) {
            return SP<CTestKeyboard>{new CTestKeyboard(std::move(name))};
        }

        const std::string& getName() override {
            return m_name;
        }

      private:
        CTestKeyboard(std::string&& name) : m_name(std::move(name)) {};

        std::string m_name;
    };

    struct STestDevice {
        SP<CTestKeyboard> keyboard;
        SP<CKeyboard>     ihid;
        std::string_view  name;
    };

    inline STestDevice makeDevice(std::string_view device_name) {
        std::string name{device_name};
        auto        keyboard = CTestKeyboard::make(std::move(name));
        auto        ihid     = SP<CKeyboard>(CKeyboard::create(std::move(keyboard)));

        ihid->m_deviceName = device_name;
        ihid->m_hlName     = device_name;

        return {
            .keyboard = std::move(keyboard),
            .ihid     = std::move(ihid),
            .name     = device_name,
        };
    }
}

TEST(KeybindsSubmap, MatchesDeviceWhenInclusive) {
    std::string   device_name = "some-keyboard";
    STestDevice   test_case   = makeDevice(device_name);
    const CSubmap SUBMAP      = makeSubmap("test", {device_name}, true);

    ASSERT_TRUE(SUBMAP.matchesDevice(test_case.ihid));
}

TEST(KeybindsSubmap, NoMatchesDeviceWhenNotInclusive) {
    std::string   device_name = "some-keyboard";
    STestDevice   test_case   = makeDevice(device_name);
    const CSubmap SUBMAP      = makeSubmap("test", {device_name}, false);

    ASSERT_FALSE(SUBMAP.matchesDevice(test_case.ihid));
}

TEST(KeybindsSubmap, NoMatchesOtherDeviceWhenInclusive) {
    std::string   device_name = "some-keyboard";
    STestDevice   test_case   = makeDevice(device_name);
    const CSubmap SUBMAP      = makeSubmap("test", {"other-keyboard"}, true);

    ASSERT_FALSE(SUBMAP.matchesDevice(test_case.ihid));
}

TEST(KeybindsSubmap, MatchesOtherDeviceWhenNotInclusive) {
    std::string   device_name = "some-keyboard";
    STestDevice   test_case   = makeDevice(device_name);
    const CSubmap SUBMAP      = makeSubmap("test", {"other-keyboard"}, false);

    ASSERT_TRUE(SUBMAP.matchesDevice(test_case.ihid));
}

TEST(KeybindsSubmap, MatchesTagWhenInclusive) {
    std::string tag_name  = "some-tag";
    STestDevice test_case = makeDevice("some-keyboard");

    test_case.ihid->m_deviceTags.emplace(tag_name);

    const CSubmap SUBMAP = makeSubmap("test", {tag_name}, true);

    ASSERT_TRUE(SUBMAP.matchesDevice(test_case.ihid));
}

TEST(KeybindsSubmap, NoMatchesTagWhenNotInclusive) {
    std::string tag_name  = "some-tag";
    STestDevice test_case = makeDevice("some-keyboard");

    test_case.ihid->m_deviceTags.emplace(tag_name);

    const CSubmap SUBMAP = makeSubmap("test", {tag_name}, false);

    ASSERT_FALSE(SUBMAP.matchesDevice(test_case.ihid));
}

TEST(KeybindsSubmap, NoMatchesDeviceWhenNoDevicesAndInclusive) {
    std::string   device_name = "some-keyboard";
    STestDevice   test_case   = makeDevice(device_name);
    const CSubmap SUBMAP      = makeSubmap("test", {}, true);

    ASSERT_FALSE(SUBMAP.matchesDevice(test_case.ihid));
}

TEST(KeybindsSubmap, MatchesDeviceWhenNoDevicesAndNotInclusive) {
    std::string   device_name = "some-keyboard";
    STestDevice   test_case   = makeDevice(device_name);
    const CSubmap SUBMAP      = makeSubmap("test", {}, false);

    ASSERT_TRUE(SUBMAP.matchesDevice(test_case.ihid));
}

TEST(KeybindsSubmap, MatchesEmptyDeviceWhenNotInclusive) {
    const CSubmap SUBMAP = makeSubmap("example", {"device_a", "device_b"}, false);
    ASSERT_TRUE(SUBMAP.matchesDevice(nullptr));
}

TEST(KeybindsSubmap, NoMatchesEmptyDeviceWhenInclusive) {
    const CSubmap SUBMAP = makeSubmap("example", {"device_a", "device_b"}, true);
    ASSERT_FALSE(SUBMAP.matchesDevice(nullptr));
}
