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
