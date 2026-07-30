#include <config/values/ConfigValues.hpp>
#include <render/blur/Provider.hpp>

#include <gtest/gtest.h>

#include <array>
#include <ranges>
#include <string_view>

using namespace Config::Values;
using namespace Render;

TEST(Config, blurVariantsMatchRendererTypes) {
    const auto VALUE = std::ranges::find_if(CONFIG_VALUES, [](const auto& value) { return std::string_view{value->name()} == "decoration:blur:variant"; });
    ASSERT_NE(VALUE, CONFIG_VALUES.end());

    const auto INT_VALUE = dynamicPointerCast<CIntValue>(*VALUE);
    ASSERT_TRUE(INT_VALUE);
    ASSERT_TRUE(INT_VALUE->m_min.has_value());
    ASSERT_TRUE(INT_VALUE->m_max.has_value());
    ASSERT_TRUE(INT_VALUE->m_map.has_value());

    EXPECT_EQ(INT_VALUE->defaultVal(), sc<Config::INTEGER>(eBlurType::BLUR_DUAL_KAWASE));
    EXPECT_EQ(*INT_VALUE->m_min, sc<Config::INTEGER>(eBlurType::BLUR_DUAL_KAWASE));
    EXPECT_EQ(*INT_VALUE->m_max, sc<Config::INTEGER>(eBlurType::BLUR_PRISM));

    const auto& MAP = *INT_VALUE->m_map;
    EXPECT_EQ(MAP.size(), 7);
    EXPECT_EQ(MAP.at("kawase"), sc<Config::INTEGER>(eBlurType::BLUR_DUAL_KAWASE));
    EXPECT_EQ(MAP.at("frost"), sc<Config::INTEGER>(eBlurType::BLUR_FROST));
    EXPECT_FALSE(MAP.contains("fluted"));
    EXPECT_FALSE(MAP.contains("hammered"));
    EXPECT_EQ(MAP.at("ripple"), sc<Config::INTEGER>(eBlurType::BLUR_RIPPLE));
    EXPECT_EQ(MAP.at("drops"), sc<Config::INTEGER>(eBlurType::BLUR_DROPS));
    EXPECT_EQ(MAP.at("water"), sc<Config::INTEGER>(eBlurType::BLUR_WATER));
    EXPECT_EQ(MAP.at("fluid_jar"), sc<Config::INTEGER>(eBlurType::BLUR_FLUID_JAR));
    EXPECT_EQ(MAP.at("prism"), sc<Config::INTEGER>(eBlurType::BLUR_PRISM));
}

TEST(Config, dropsAnimationSpeedIsBounded) {
    const auto VALUE = std::ranges::find_if(CONFIG_VALUES, [](const auto& value) { return std::string_view{value->name()} == "decoration:blur:drops:speed"; });
    ASSERT_NE(VALUE, CONFIG_VALUES.end());

    const auto FLOAT_VALUE = dynamicPointerCast<CFloatValue>(*VALUE);
    ASSERT_TRUE(FLOAT_VALUE);
    ASSERT_TRUE(FLOAT_VALUE->m_min.has_value());
    ASSERT_TRUE(FLOAT_VALUE->m_max.has_value());

    EXPECT_FLOAT_EQ(FLOAT_VALUE->defaultVal(), 3.F);
    EXPECT_FLOAT_EQ(*FLOAT_VALUE->m_min, 0.F);
    EXPECT_FLOAT_EQ(*FLOAT_VALUE->m_max, 10.F);
}

TEST(Config, waterSettingsAreBounded) {
    struct SWaterSetting {
        std::string_view name;
        float            defaultValue;
        float            min;
        float            max;
    };

    constexpr std::array SETTINGS = {
        SWaterSetting{"decoration:blur:water:strength", 32.F, 0.F, 32.F},  SWaterSetting{"decoration:blur:water:radius", 20.F, 1.F, 1000.F},
        SWaterSetting{"decoration:blur:water:speed", 0.76F, 0.F, 10.F},    SWaterSetting{"decoration:blur:water:damping", 0.95F, 0.F, 1.F},
        SWaterSetting{"decoration:blur:water:duration", 12.F, 0.5F, 60.F},
    };

    for (const auto& setting : SETTINGS) {
        const auto VALUE = std::ranges::find_if(CONFIG_VALUES, [&setting](const auto& value) { return std::string_view{value->name()} == setting.name; });
        ASSERT_NE(VALUE, CONFIG_VALUES.end());

        const auto FLOAT_VALUE = dynamicPointerCast<CFloatValue>(*VALUE);
        ASSERT_TRUE(FLOAT_VALUE);
        ASSERT_TRUE(FLOAT_VALUE->m_min.has_value());
        ASSERT_TRUE(FLOAT_VALUE->m_max.has_value());

        EXPECT_FLOAT_EQ(FLOAT_VALUE->defaultVal(), setting.defaultValue);
        EXPECT_FLOAT_EQ(*FLOAT_VALUE->m_min, setting.min);
        EXPECT_FLOAT_EQ(*FLOAT_VALUE->m_max, setting.max);
        EXPECT_EQ(FLOAT_VALUE->refreshBits(), Config::Supplementary::REFRESH_BLUR_FB);
    }
}

TEST(Config, fluidJarSettingsAreBounded) {
    const auto COLOR = std::ranges::find_if(CONFIG_VALUES, [](const auto& value) { return std::string_view{value->name()} == "decoration:blur:fluid_jar:color"; });
    ASSERT_NE(COLOR, CONFIG_VALUES.end());

    const auto COLOR_VALUE = dynamicPointerCast<CColorValue>(*COLOR);
    ASSERT_TRUE(COLOR_VALUE);
    EXPECT_EQ(COLOR_VALUE->defaultVal(), 0xCC3399FF);
    EXPECT_EQ(COLOR_VALUE->refreshBits(), Config::Supplementary::REFRESH_BLUR_FB);

    struct SFluidJarSetting {
        std::string_view name;
        float            defaultValue;
        float            min;
        float            max;
    };

    constexpr std::array SETTINGS = {
        SFluidJarSetting{"decoration:blur:fluid_jar:speed", 3.7F, 0.F, 10.F},     SFluidJarSetting{"decoration:blur:fluid_jar:fill_amount", 0.5F, 0.F, 1.F},
        SFluidJarSetting{"decoration:blur:fluid_jar:mass", 1.4F, 0.1F, 10.F},     SFluidJarSetting{"decoration:blur:fluid_jar:precision", 2.F, 0.5F, 8.F},
        SFluidJarSetting{"decoration:blur:fluid_jar:turbulence", 1.2F, 0.F, 5.F}, SFluidJarSetting{"decoration:blur:fluid_jar:distortion", 8.F, 0.F, 10.F},
    };

    for (const auto& setting : SETTINGS) {
        const auto VALUE = std::ranges::find_if(CONFIG_VALUES, [&setting](const auto& value) { return std::string_view{value->name()} == setting.name; });
        ASSERT_NE(VALUE, CONFIG_VALUES.end());

        const auto FLOAT_VALUE = dynamicPointerCast<CFloatValue>(*VALUE);
        ASSERT_TRUE(FLOAT_VALUE);
        ASSERT_TRUE(FLOAT_VALUE->m_min.has_value());
        ASSERT_TRUE(FLOAT_VALUE->m_max.has_value());

        EXPECT_FLOAT_EQ(FLOAT_VALUE->defaultVal(), setting.defaultValue);
        EXPECT_FLOAT_EQ(*FLOAT_VALUE->m_min, setting.min);
        EXPECT_FLOAT_EQ(*FLOAT_VALUE->m_max, setting.max);
        EXPECT_EQ(FLOAT_VALUE->refreshBits(), Config::Supplementary::REFRESH_BLUR_FB);
    }
}
