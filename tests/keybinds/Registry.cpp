#include <keybinds/Registry.hpp>

#include <gtest/gtest.h>

using namespace Keybinds;
using namespace Input;

static CBind makeRegistryBind(std::string displayKey, std::string submap = {}) {
    auto result = CBind::make({"SUPER", "K"}, sc<BindFlags>(0), [] { return SBindResult{}; },
                              {
                                  .metadata =
                                      {
                                          .displayKey = std::move(displayKey),
                                          .submap     = std::move(submap),
                                      },
                              });
    EXPECT_TRUE(result.has_value());
    return std::move(*result);
}

static CSubmap makeSubmap(std::string name, std::unordered_set<std::string> devices, const bool inclusive) {
    return CSubmap(name, {.devices = devices, .inclusive = inclusive});
};

TEST(KeybindsRegistry, RemovesExactBind) {
    CRegistry  registry;
    const auto FIRST  = registry.add(makeRegistryBind("SUPER + K"));
    const auto SECOND = registry.add(makeRegistryBind("SUPER + K"));

    EXPECT_TRUE(registry.remove(FIRST));
    ASSERT_EQ(registry.size(), 1);
    EXPECT_EQ(registry.binds().front(), SECOND);
}

TEST(KeybindsRegistry, RemovesNormalizedDisplayKey) {
    CRegistry registry;
    registry.add(makeRegistryBind("SUPER + K"));
    registry.add(makeRegistryBind("super+k"));
    registry.add(makeRegistryBind("SUPER + A"));

    EXPECT_EQ(registry.removeByDisplayKey(" SuPeR + k "), 2);
    ASSERT_EQ(registry.size(), 1);
    EXPECT_EQ(registry.binds().front()->metadata().displayKey, "SUPER + A");
}

TEST(KeybindsRegistry, FindsSubmap) {
    CRegistry registry;
    registry.add(makeRegistryBind("SUPER + K", "resize"));

    EXPECT_TRUE(registry.hasSubmap("resize"));
    EXPECT_FALSE(registry.hasSubmap("missing"));
}

TEST(KeybindsRegistry, FindsSubmapNewAPI) {
    CRegistry registry;
    registry.addSubmap(makeSubmap("example", {}, false));

    EXPECT_TRUE(registry.findSubmap("example", nullptr));
}

TEST(KeybindsRegistry, FindsShortcutWithSidedModifier) {
    CRegistry registry;
    auto      result = CBind::make({"SHIFT_L", "K"}, sc<BindFlags>(0), [] { return SBindResult{}; });
    ASSERT_TRUE(result.has_value());
    const auto BIND = registry.add(std::move(*result));

    EXPECT_EQ(registry.findShortcutConflict(XKB_KEY_k, HL_MODIFIER_SHIFT), BIND);
}
