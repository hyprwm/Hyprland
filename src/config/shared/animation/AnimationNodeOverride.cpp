#include "AnimationNodeOverride.hpp"

#include "AnimationTree.hpp"

#include "../../../debug/log/Logger.hpp"
#include "../../../helpers/MiscFunctions.hpp"
#include "../../../managers/animation/AnimationManager.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>

using namespace Hyprutils::String;

namespace Config {

    // Parse the animation override string.
    std::optional<std::string> parseWindowRuleAnimationValue(std::string_view value, std::string_view nodeName, SWindowRuleAnimationParseResult& out) {

        // Trim the value and split into tokens.
        const auto TRIM = trim(std::string{value});
        CVarList2  tokens(std::string(TRIM), 0, 's');
        if (tokens.size() == 0)
            return "empty animation override";

        // Parse the enabled state.
        const auto enabledE = configStringToInt(std::string(tokens[0]));
        if (!enabledE)
            return enabledE.error();

        const int64_t ev = *enabledE;
        if (ev != 0 && ev != 1)
            return "invalid animation on/off state";

        if (ev == 0) {
            out.disabled = true;
            return std::nullopt;
        }

        // Exit early if there are not enough tokens.
        if (tokens.size() < 3)
            return "expected enabled speed bezier [style...]";

        // Parse the speed.
        float speed = 0.F;
        try {
            speed = std::stof(std::string(tokens[1]));
        } catch (...) { return "invalid speed"; }

        if (speed <= 0.F)
            return "invalid speed";

        // Parse the bezier.
        std::string bezier = std::string(tokens[2]);
        if (!g_pAnimationManager->bezierExists(bezier))
            return "no such bezier";

        // Parse the style and all remaining tokens as options
        std::string style;
        if (tokens.size() > 3) {
            for (size_t i = 3; i < tokens.size(); ++i) {
                if (i > 3)
                    style += ' ';
                style += tokens[i];
            }
        }

        if (!style.empty()) {
            auto err = g_pAnimationManager->styleValidInConfigVar(std::string(nodeName), style);
            if (!err.empty())
                return err;
        }

        out.disabled = false;
        out.speed    = speed;
        out.bezier   = std::move(bezier);
        out.style    = std::move(style);
        return std::nullopt;
    }

    // Instantiate standalone animation config from parsed input.
    SP<Hyprutils::Animation::SAnimationPropertyConfig> makeStandaloneAnimationConfig(const SWindowRuleAnimationParseResult& parsed, const std::string& nodeName) {

        auto cfg = makeShared<Hyprutils::Animation::SAnimationPropertyConfig>();

        if (parsed.disabled) {
            cfg->internalEnabled = 0;
            cfg->internalSpeed   = 1.F;
            cfg->internalBezier  = "default";
            cfg->internalStyle   = "";
        } else {
            cfg->internalEnabled = 1;
            cfg->internalSpeed   = parsed.speed;
            cfg->internalBezier  = parsed.bezier;
            cfg->internalStyle   = parsed.style;
        }

        cfg->overridden       = true;
        const auto base       = animationTree()->getAnimationPropertyConfig(nodeName);
        cfg->pParentAnimation = base->pParentAnimation;
        cfg->pValues          = cfg;
        return cfg;
    }

    // Parse and return animation config on success; on parse error logs and returns global.
    SP<Hyprutils::Animation::SAnimationPropertyConfig> windowAnimationConfigForNode(std::optional<std::string> overrideStr, const std::string& nodeName) {
        // If no override string, use the global tree node.
        if (!overrideStr.has_value() || trim(*overrideStr).empty())
            return animationTree()->getAnimationPropertyConfig(nodeName);

        // Parse the override string.
        SWindowRuleAnimationParseResult parsed;
        if (const auto err = parseWindowRuleAnimationValue(trim(*overrideStr), nodeName, parsed)) {
            Log::logger->log(Log::ERR, "window rule animation {}: {}", nodeName, *err);
            return animationTree()->getAnimationPropertyConfig(nodeName);
        }

        return makeStandaloneAnimationConfig(parsed, nodeName);
    }

    // Highest-priority non-empty string from applicator vars (skips empty setprop so window rules are not shadowed by "").
    std::optional<std::string> animOverrideString(const Desktop::Types::COverridableVar<std::string>& v) {
        using namespace Desktop::Types;
        for (int i = static_cast<int>(PRIORITY_SET_PROP); i >= 0; --i) {
            const auto o = v.valueAt(static_cast<eOverridePriority>(i));
            if (!o.has_value())
                continue;
            const auto t = trim(*o);
            if (!t.empty())
                return std::string{t};
        }
        return std::nullopt;
    }

}
