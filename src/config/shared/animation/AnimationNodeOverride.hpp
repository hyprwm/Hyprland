#pragma once

#include "../../../desktop/types/OverridableVar.hpp"
#include "../../../helpers/memory/Memory.hpp"

#include <hyprutils/animation/AnimationConfig.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace Config {

    struct SWindowRuleAnimationParseResult {
        bool        disabled = false;
        float       speed    = 10.F;
        std::string bezier   = "default";
        std::string style;
    };

    // Parses space-separated: enabled speed bezier [style tokens...].
    [[nodiscard]] std::optional<std::string> parseWindowRuleAnimationValue(std::string_view value, std::string_view nodeName, SWindowRuleAnimationParseResult& out);

    // Instantiate standalone animation config from parsed result.
    [[nodiscard]] SP<Hyprutils::Animation::SAnimationPropertyConfig> makeStandaloneAnimationConfig(const SWindowRuleAnimationParseResult& parsed, const std::string& nodeName);

    // Parse and return animation config on success; on parse error logs and returns global.
    [[nodiscard]] SP<Hyprutils::Animation::SAnimationPropertyConfig> windowAnimationConfigForNode(std::optional<std::string> overrideStr, const std::string& nodeName);

    // Highest-priority non-empty string from applicator vars (skips empty setprop so window rules are not shadowed by "").
    [[nodiscard]] std::optional<std::string> animOverrideString(const Desktop::Types::COverridableVar<std::string>& v);

}
