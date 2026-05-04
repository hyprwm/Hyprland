#pragma once

#include <vector>
#include <limits>
#include <expected>
#include <format>
#include <utility>

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/Color.hpp"

#include "./types/IValue.hpp"
#include "./types/BoolValue.hpp"
#include "./types/ColorValue.hpp"
#include "./types/CssGapValue.hpp"
#include "./types/FloatValue.hpp"
#include "./types/FontWeightValue.hpp"
#include "./types/GradientValue.hpp"
#include "./types/IntValue.hpp"
#include "./types/StringValue.hpp"
#include "./types/Vec2Value.hpp"

namespace Config::Values {

    inline auto vec2Range(float minX, float minY, float maxX, float maxY) {
        return [=](const Config::VEC2& v) -> std::expected<void, std::string> {
            if (v.x < minX || v.x > maxX || v.y < minY || v.y > maxY)
                return std::unexpected(std::format("value out of range [{}, {}] - [{}, {}]", minX, minY, maxX, maxY));
            return {};
        };
    }

    inline auto strChoice(std::vector<std::string> rawVec) {
        return [vec = std::move(rawVec)](const Config::STRING& v) -> std::expected<void, std::string> {
            if (!std::ranges::contains(vec, v)) {
                std::string allowed = "";
                for (const auto& e : vec) {
                    allowed += std::format("\"{}\", ", e);
                }

                allowed = allowed.substr(0, allowed.size() - 2);

                return std::unexpected(std::format("bad value \"{}\", allowed values are: {}", v, allowed));
            }
            return {};
        };
    }

    using Bool       = CBoolValue;
    using Color      = CColorValue;
    using CssGap     = CCssGapValue;
    using Float      = CFloatValue;
    using FontWeight = CFontWeightValue;
    using Gradient   = CGradientValue;
    using Int        = CIntValue;
    using String     = CStringValue;
    using Vec2       = CVec2Value;

    template <typename T>
    struct SValueOptions;

    template <>
    struct SValueOptions<CBoolValue> {
        using type = SBoolValueOptions;
    };

    template <>
    struct SValueOptions<CColorValue> {
        using type = SColorValueOptions;
    };

    template <>
    struct SValueOptions<CCssGapValue> {
        using type = SCssGapValueOptions;
    };

    template <>
    struct SValueOptions<CFloatValue> {
        using type = SFloatValueOptions;
    };

    template <>
    struct SValueOptions<CFontWeightValue> {
        using type = SFontWeightValueOptions;
    };

    template <>
    struct SValueOptions<CGradientValue> {
        using type = SGradientValueOptions;
    };

    template <>
    struct SValueOptions<CIntValue> {
        using type = SIntValueOptions;
    };

    template <>
    struct SValueOptions<CStringValue> {
        using type = SStringValueOptions;
    };

    template <>
    struct SValueOptions<CVec2Value> {
        using type = SVec2ValueOptions;
    };

    template <typename T>
    using valueOptions_t = typename SValueOptions<T>::type;

    template <typename T, typename Def>
    SP<T> makeConfigValue(const char* name, const char* description, Def&& def, valueOptions_t<T> options) {
        return makeShared<T>(name, description, std::forward<Def>(def), std::move(options));
    }

    template <typename T, typename... Args>
    SP<T> makeConfigValue(Args&&... args) {
        return makeShared<T>(std::forward<Args>(args)...);
    }

    using OptionMap = std::unordered_map<std::string, Config::INTEGER>;

    // Device config values follow the parents. They have the same name,
    // and restrictions, except without the category.
    inline const std::vector<std::string> CONFIG_DEVICE_VALUE_NAMES = {
        "input:sensitivity",
        "input:accel_profile",
        "input:rotation",
        "input:kb_file",
        "input:kb_layout",
        "input:kb_variant",
        "input:kb_options",
        "input:kb_rules",
        "input:kb_model",
        "input:repeat_rate",
        "input:repeat_delay",
        "input:natural_scroll",
        "input:touchpad:natural_scroll",
        "input:touchpad:tap_button_map",
        "input:numlock_by_default",
        "input:resolve_binds_by_sym",
        "input:touchpad:disable_while_typing",
        "input:touchpad:clickfinger_behavior",
        "input:touchpad:middle_button_emulation",
        "input:touchpad:tap-to-click",
        "input:touchpad:tap-and-drag",
        "input:touchpad:drag_lock",
        "input:left_handed",
        "input:tablet:left_handed",
        "input:scroll_method",
        "input:scroll_button",
        "input:scroll_button_lock",
        "input:scroll_points",
        "input:scroll_factor",
        "input:touchpad:scroll_factor",
        "input:touchdevice:transform",
        "input:tablet:transform",
        "input:touchdevice:output",
        "input:tablet:output",
        "input:touchdevice:enabled",
        "input:tablet:region_position",
        "input:tablet:absolute_region_position",
        "input:tablet:region_size",
        "input:tablet:relative_input",
        "input:tablet:active_area_position",
        "input:tablet:active_area_size",
        "input:tablettool:eraser_button_mode",
        "input:tablettool:eraser_button_override",
        "input:tablettool:pressure_range_min",
        "input:tablettool:pressure_range_max",
        "input:touchpad:flip_x",
        "input:touchpad:flip_y",
        "input:touchpad:drag_3fg",
        "input:virtualkeyboard:share_states",
        "input:virtualkeyboard:release_pressed_on_close",
    };

    std::vector<SP<IValue>>              getConfigValues();
    inline const std::vector<SP<IValue>> CONFIG_VALUES = getConfigValues();

    std::string                          getAsJson();
};
