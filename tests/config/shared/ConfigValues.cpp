#include <config/values/ConfigValues.hpp>

#include <gtest/gtest.h>

#include <ranges>

TEST(ConfigValues, hasOverviewBorderColors) {
    const auto findColor = [](std::string_view name) -> const Config::Values::CColorValue* {
        const auto IT = std::ranges::find_if(Config::Values::CONFIG_VALUES, [name](const auto& value) { return std::string_view{value->name()} == name; });
        return IT == Config::Values::CONFIG_VALUES.end() ? nullptr : dc<const Config::Values::CColorValue*>(IT->get());
    };

    const auto* ACTIVE   = findColor("overview:col.active_border");
    const auto* INACTIVE = findColor("overview:col.inactive_border");

    ASSERT_NE(ACTIVE, nullptr);
    ASSERT_NE(INACTIVE, nullptr);
    EXPECT_EQ(ACTIVE->defaultVal(), 0xffffffff);
    EXPECT_EQ(INACTIVE->defaultVal(), 0xff444444);
}

TEST(ConfigValues, hasOverviewSearchModes) {
    const auto findString = [](std::string_view name) -> const Config::Values::CStringValue* {
        const auto IT = std::ranges::find_if(Config::Values::CONFIG_VALUES, [name](const auto& value) { return std::string_view{value->name()} == name; });
        return IT == Config::Values::CONFIG_VALUES.end() ? nullptr : dc<const Config::Values::CStringValue*>(IT->get());
    };

    const auto findInt = [](std::string_view name) -> const Config::Values::CIntValue* {
        const auto IT = std::ranges::find_if(Config::Values::CONFIG_VALUES, [name](const auto& value) { return std::string_view{value->name()} == name; });
        return IT == Config::Values::CONFIG_VALUES.end() ? nullptr : dc<const Config::Values::CIntValue*>(IT->get());
    };

    const auto* WINDOW_PREFIX    = findString("overview:search:window_prefix");
    const auto* WORKSPACE_PREFIX = findString("overview:search:workspace_prefix");
    const auto* DEFAULT_MODE     = findInt("overview:search:default_mode");

    ASSERT_NE(WINDOW_PREFIX, nullptr);
    ASSERT_NE(WORKSPACE_PREFIX, nullptr);
    ASSERT_NE(DEFAULT_MODE, nullptr);
    EXPECT_EQ(WINDOW_PREFIX->defaultVal(), "/");
    EXPECT_EQ(WORKSPACE_PREFIX->defaultVal(), ".");
    EXPECT_EQ(DEFAULT_MODE->defaultVal(), 0);

    EXPECT_TRUE(WINDOW_PREFIX->validator()("/").has_value());
    EXPECT_FALSE(WINDOW_PREFIX->validator()("").has_value());
    EXPECT_FALSE(WINDOW_PREFIX->validator()("//").has_value());
    ASSERT_TRUE(DEFAULT_MODE->m_map.has_value());
    EXPECT_EQ(DEFAULT_MODE->m_map->at("all"), 0);
    EXPECT_EQ(DEFAULT_MODE->m_map->at("window"), 1);
    EXPECT_EQ(DEFAULT_MODE->m_map->at("workspace"), 2);
}
