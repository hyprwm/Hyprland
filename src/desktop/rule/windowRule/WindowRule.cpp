#include "WindowRule.hpp"
#include "../../view/Window.hpp"
#include "../../../helpers/Monitor.hpp"
#include "../../../helpers/MiscFunctions.hpp"
#include "../../../Compositor.hpp"
#include "../../../managers/TokenManager.hpp"
#include "../../../desktop/state/FocusState.hpp"
#include "../../../protocols/types/ContentType.hpp"
#include "../../../config/shared/parserUtils/ParserUtils.hpp"

#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/VarList2.hpp>
#include <algorithm>
#include <format>

using namespace Desktop;
using namespace Desktop::Rule;
using namespace Hyprutils::String;

static const char* numericParseError(eNumericParseResult r) {
    switch (r) {
        case NUMERIC_PARSE_BAD: return "bad input";
        case NUMERIC_PARSE_GARBAGE: return "garbage input";
        case NUMERIC_PARSE_OUT_OF_RANGE: return "out of range";
        case NUMERIC_PARSE_OK: return "ok";
        default: return "error";
    }
}

static std::expected<int64_t, std::string> parseInt(std::string_view effectName, const std::string& raw) {
    auto parsed = strToNumber<int64_t>(raw);
    if (!parsed)
        return std::unexpected(std::format("{} rule \"{}\" failed with: {}", effectName, raw, numericParseError(parsed.error())));

    return *parsed;
}

static std::expected<float, std::string> parseFloat(std::string_view effectName, const std::string& raw) {
    auto parsed = strToNumber<float>(raw);
    if (!parsed)
        return std::unexpected(std::format("{} rule \"{}\" failed with: {}", effectName, raw, numericParseError(parsed.error())));

    return *parsed;
}

static std::expected<CHyprColor, std::string> parseBorderColorToken(const std::string& raw, const std::string& token) {
    auto parsed = Config::ParserUtils::parseColor(token);
    if (!parsed)
        return std::unexpected(std::format(R"(border_color rule "{}" has invalid color "{}": {})", raw, token, parsed.error()));

    return CHyprColor(*parsed);
}

static std::expected<SFullscreenStateRule, std::string> parseFullscreenState(const std::string& raw) {
    CVarList2            vars(std::string{raw}, 0, 's');

    SFullscreenStateRule result;

    auto                 internal = strToNumber<int>(vars[0]);
    if (!internal)
        return std::unexpected(std::format("fullscreen_state rule \"{}\" failed with: {}", raw, numericParseError(internal.error())));

    result.internal = *internal;

    if (!vars[1].empty()) {
        auto client = strToNumber<int>(vars[1]);
        if (!client)
            return std::unexpected(std::format("fullscreen_state rule \"{}\" failed with: {}", raw, numericParseError(client.error())));

        result.client = *client;
    }

    return result;
}

static std::expected<int64_t, std::string> parseIdleInhibitMode(const std::string& raw) {
    if (raw == "none")
        return IDLEINHIBIT_NONE;
    if (raw == "always")
        return IDLEINHIBIT_ALWAYS;
    if (raw == "focus")
        return IDLEINHIBIT_FOCUS;
    if (raw == "fullscreen")
        return IDLEINHIBIT_FULLSCREEN;

    return std::unexpected(std::format("idle_inhibit rule has unknown mode \"{}\"", raw));
}

static std::expected<SOpacityRule, std::string> parseOpacityRule(const std::string& raw) {
    try {
        CVarList2    vars(std::string{raw}, 0, ' ');

        int          opacityIDX = 0;
        SOpacityRule result;

        for (const auto& r : vars) {
            if (r == "opacity")
                continue;

            if (r == "override") {
                if (opacityIDX == 1)
                    result.alpha.overridden = true;
                else if (opacityIDX == 2)
                    result.alphaInactive.overridden = true;
                else if (opacityIDX == 3)
                    result.alphaFullscreen.overridden = true;
            } else {
                auto alpha = strToNumber<float>(r);
                if (!alpha)
                    return std::unexpected(std::format("opacity rule \"{}\" failed with: {}", raw, numericParseError(alpha.error())));

                if (opacityIDX == 0)
                    result.alpha.alpha = *alpha;
                else if (opacityIDX == 1)
                    result.alphaInactive.alpha = *alpha;
                else if (opacityIDX == 2)
                    result.alphaFullscreen.alpha = *alpha;
                else
                    throw std::runtime_error("more than 3 alpha values");

                opacityIDX++;
            }
        }

        if (opacityIDX == 1) {
            result.alphaInactive   = result.alpha;
            result.alphaFullscreen = result.alpha;
        }

        return result;
    } catch (std::exception& e) { return std::unexpected(std::format("opacity rule \"{}\" failed with: {}", raw, e.what())); }
}

static std::expected<SBorderColorRule, std::string> parseBorderColorRule(const std::string& raw) {
    try {
        Config::CGradientValueData activeBorderGradient   = {};
        Config::CGradientValueData inactiveBorderGradient = {};
        bool                       active                 = true;
        CVarList                   colorsAndAngles        = CVarList(trim(raw), 0, 's', true);

        if (colorsAndAngles.size() == 2 && !colorsAndAngles[1].contains("deg")) {
            auto activeColor = parseBorderColorToken(raw, colorsAndAngles[0]);
            if (!activeColor)
                return std::unexpected(activeColor.error());

            auto inactiveColor = parseBorderColorToken(raw, colorsAndAngles[1]);
            if (!inactiveColor)
                return std::unexpected(inactiveColor.error());

            return SBorderColorRule{
                .active   = Config::CGradientValueData(*activeColor),
                .inactive = Config::CGradientValueData(*inactiveColor),
            };
        }

        for (auto const& token : colorsAndAngles) {
            if (active && token.contains("deg")) {
                auto angle = strToNumber<int>(token.substr(0, token.size() - 3));
                if (!angle)
                    return std::unexpected(std::format("border_color rule \"{}\" has invalid angle \"{}\": {}", raw, token, numericParseError(angle.error())));

                activeBorderGradient.m_angle = *angle * (PI / 180.0);
                active                       = false;
            } else if (token.contains("deg")) {
                auto angle = strToNumber<int>(token.substr(0, token.size() - 3));
                if (!angle)
                    return std::unexpected(std::format("border_color rule \"{}\" has invalid angle \"{}\": {}", raw, token, numericParseError(angle.error())));

                inactiveBorderGradient.m_angle = *angle * (PI / 180.0);
            } else {
                auto color = parseBorderColorToken(raw, token);
                if (!color)
                    return std::unexpected(color.error());

                if (active)
                    activeBorderGradient.m_colors.emplace_back(*color);
                else
                    inactiveBorderGradient.m_colors.emplace_back(*color);
            }
        }

        activeBorderGradient.updateColorsOk();

        if (activeBorderGradient.m_colors.size() > 10 || inactiveBorderGradient.m_colors.size() > 10)
            return std::unexpected(std::format("border_color rule \"{}\" has more than 10 colors in one gradient", raw));
        if (activeBorderGradient.m_colors.empty())
            return std::unexpected(std::format("border_color rule \"{}\" has no colors", raw));

        SBorderColorRule result{.active = activeBorderGradient};
        if (!inactiveBorderGradient.m_colors.empty())
            result.inactive = inactiveBorderGradient;

        return result;
    } catch (std::exception& e) { return std::unexpected(std::format("border_color rule \"{}\" failed with: {}", raw, e.what())); }
}

static std::vector<std::string> parseStringList(const std::string& raw) {
    std::vector<std::string> result;
    CVarList2                varlist(std::string{raw}, 0, 's');

    for (const auto& e : varlist) {
        result.emplace_back(e);
    }

    return result;
}

static std::expected<WindowRuleEffectValue, std::string> parseWindowRuleEffect(CWindowRuleEffectContainer::storageType e, const std::string& raw) {
    if (windowEffects()->isEffectDynamic(e))
        return std::string{raw};

    const auto EFFECT_NAME = windowEffects()->get(e);

    switch (e) {
        default: return std::unexpected(std::format("unknown window rule effect {}", e));

        case WINDOW_RULE_EFFECT_NONE: return std::monostate{};

        case WINDOW_RULE_EFFECT_FLOAT:
        case WINDOW_RULE_EFFECT_TILE:
        case WINDOW_RULE_EFFECT_FULLSCREEN:
        case WINDOW_RULE_EFFECT_MAXIMIZE:
        case WINDOW_RULE_EFFECT_CENTER:
        case WINDOW_RULE_EFFECT_PSEUDO:
        case WINDOW_RULE_EFFECT_NOINITIALFOCUS:
        case WINDOW_RULE_EFFECT_PIN:
        case WINDOW_RULE_EFFECT_PERSISTENT_SIZE:
        case WINDOW_RULE_EFFECT_ALLOWS_INPUT:
        case WINDOW_RULE_EFFECT_DIM_AROUND:
        case WINDOW_RULE_EFFECT_DECORATE:
        case WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE:
        case WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO:
        case WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR:
        case WINDOW_RULE_EFFECT_NO_ANIM:
        case WINDOW_RULE_EFFECT_NO_BLUR:
        case WINDOW_RULE_EFFECT_NO_DIM:
        case WINDOW_RULE_EFFECT_NO_FOCUS:
        case WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE:
        case WINDOW_RULE_EFFECT_NO_MAX_SIZE:
        case WINDOW_RULE_EFFECT_NO_SHADOW:
        case WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT:
        case WINDOW_RULE_EFFECT_OPAQUE:
        case WINDOW_RULE_EFFECT_FORCE_RGBX:
        case WINDOW_RULE_EFFECT_SYNC_FULLSCREEN:
        case WINDOW_RULE_EFFECT_IMMEDIATE:
        case WINDOW_RULE_EFFECT_XRAY:
        case WINDOW_RULE_EFFECT_RENDER_UNFOCUSED:
        case WINDOW_RULE_EFFECT_NO_SCREEN_SHARE:
        case WINDOW_RULE_EFFECT_NO_VRR:
        case WINDOW_RULE_EFFECT_CONFINE_POINTER:
        case WINDOW_RULE_EFFECT_STAY_FOCUSED: return truthy(raw);

        case WINDOW_RULE_EFFECT_FULLSCREENSTATE: {
            auto parsed = parseFullscreenState(raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return *parsed;
        }

        case WINDOW_RULE_EFFECT_MOVE:
        case WINDOW_RULE_EFFECT_SIZE:
        case WINDOW_RULE_EFFECT_MAX_SIZE:
        case WINDOW_RULE_EFFECT_MIN_SIZE: {
            auto parsed = Math::parseExpressionVec2(raw);
            if (!parsed)
                return std::unexpected(std::format("{} rule \"{}\" failed with: {}", EFFECT_NAME, raw, parsed.error()));
            return *parsed;
        }

        case WINDOW_RULE_EFFECT_MONITOR:
        case WINDOW_RULE_EFFECT_WORKSPACE:
        case WINDOW_RULE_EFFECT_GROUP:
        case WINDOW_RULE_EFFECT_ANIMATION:
        case WINDOW_RULE_EFFECT_TAG: return std::string{raw};

        case WINDOW_RULE_EFFECT_SUPPRESSEVENT: return parseStringList(raw);

        case WINDOW_RULE_EFFECT_CONTENT: return sc<int64_t>(NContentType::fromString(raw));

        case WINDOW_RULE_EFFECT_NOCLOSEFOR:
        case WINDOW_RULE_EFFECT_BORDER_SIZE: {
            auto parsed = parseInt(EFFECT_NAME, raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return *parsed;
        }

        case WINDOW_RULE_EFFECT_ROUNDING: {
            auto parsed = parseInt(EFFECT_NAME, raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            if (*parsed < 0)
                return std::unexpected(std::format("{} rule \"{}\" must be non-negative", EFFECT_NAME, raw));
            return *parsed;
        }

        case WINDOW_RULE_EFFECT_SCROLLING_WIDTH: {
            auto parsed = parseFloat(EFFECT_NAME, raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return *parsed;
        }

        case WINDOW_RULE_EFFECT_ROUNDING_POWER: {
            auto parsed = parseFloat(EFFECT_NAME, raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return std::clamp(*parsed, 1.F, 10.F);
        }
        case WINDOW_RULE_EFFECT_SCROLL_MOUSE:
        case WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD: {
            auto parsed = parseFloat(EFFECT_NAME, raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return std::clamp(*parsed, 0.01F, 10.F);
        }

        case WINDOW_RULE_EFFECT_BORDER_COLOR: {
            auto parsed = parseBorderColorRule(raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return *parsed;
        }
        case WINDOW_RULE_EFFECT_IDLE_INHIBIT: {
            auto parsed = parseIdleInhibitMode(raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return *parsed;
        }
        case WINDOW_RULE_EFFECT_OPACITY: {
            auto parsed = parseOpacityRule(raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return *parsed;
        }
    }
}

CWindowRule::CWindowRule(const std::string& name) : CRuleWithEffects<SWindowRuleEffect, RULE_TYPE_WINDOW>(name) {
    ;
}

std::expected<void, std::string> CWindowRule::addEffect(CWindowRule::storageType e, const Math::SExpressionVec2& expr) {
    switch (e) {
        case WINDOW_RULE_EFFECT_MOVE:
        case WINDOW_RULE_EFFECT_SIZE:
        case WINDOW_RULE_EFFECT_MAX_SIZE:
        case WINDOW_RULE_EFFECT_MIN_SIZE: break;
        default: return std::unexpected(std::format("{} is not an expression vec2 window rule effect", windowEffects()->get(e)));
    }

    return addParsedEffect(e, expr, expr.toString());
}

std::expected<WindowRuleEffectValue, std::string> CWindowRule::parseEffect(CWindowRule::storageType e, const std::string& result) {
    return parseWindowRuleEffect(e, result);
}

bool CWindowRule::matches(PHLWINDOW w, bool allowEnvLookup) {
    if (!canMatch())
        return false;

    for (const auto& [prop, engine] : m_matchEngines) {
        switch (prop) {
            default: {
                Log::logger->log(Log::TRACE, "CWindowRule::matches: skipping prop entry {}", sc<std::underlying_type_t<eRuleProperty>>(prop));
                break;
            }

            case RULE_PROP_TITLE:
                if (!engine->match(w->m_title))
                    return false;
                break;
            case RULE_PROP_INITIAL_TITLE:
                if (!engine->match(w->m_initialTitle))
                    return false;
                break;
            case RULE_PROP_CLASS:
                if (!engine->match(w->m_class))
                    return false;
                break;
            case RULE_PROP_INITIAL_CLASS:
                if (!engine->match(w->m_initialClass))
                    return false;
                break;
            case RULE_PROP_FLOATING:
                if (!engine->match(w->m_isFloating))
                    return false;
                break;
            case RULE_PROP_TAG:
                if (!engine->match(w->m_ruleApplicator->m_tagKeeper))
                    return false;
                break;
            case RULE_PROP_XWAYLAND:
                if (!engine->match(w->m_isX11))
                    return false;
                break;
            case RULE_PROP_FULLSCREEN:
                if (!engine->match(w->m_fullscreenState.internal != 0))
                    return false;
                break;
            case RULE_PROP_PINNED:
                if (!engine->match(w->m_pinned))
                    return false;
                break;
            case RULE_PROP_FOCUS:
                if (!engine->match(Desktop::focusState()->window() == w))
                    return false;
                break;
            case RULE_PROP_GROUP:
                if (!engine->match(!!w->m_group))
                    return false;
                break;
            case RULE_PROP_MODAL:
                if (!engine->match(w->isModal()))
                    return false;
                break;
            case RULE_PROP_FULLSCREENSTATE_INTERNAL:
                if (!engine->match(w->m_fullscreenState.internal))
                    return false;
                break;
            case RULE_PROP_FULLSCREENSTATE_CLIENT:
                if (!engine->match(w->m_fullscreenState.client))
                    return false;
                break;
            case RULE_PROP_ON_WORKSPACE:
                if (!engine->match(w->m_workspace))
                    return false;
                break;
            case RULE_PROP_CONTENT:
                if (!engine->match(std::format("{}", sc<size_t>(w->getContentType()))) && !engine->match(NContentType::toString(w->getContentType())))
                    return false;
                break;
            case RULE_PROP_XDG_TAG:
                if (!w->xdgTag().has_value() || !engine->match(*w->xdgTag()))
                    return false;
                break;

            case RULE_PROP_EXEC_TOKEN:
                if (!allowEnvLookup)
                    break;

                const auto ENV   = w->getEnv();
                bool       match = false;

                if (ENV.contains(EXEC_RULE_ENV_NAME)) {
                    if (engine->match(ENV.at(EXEC_RULE_ENV_NAME)))
                        match = true;
                } else if (m_matchEngines.contains(RULE_PROP_EXEC_PID)) {
                    if (m_matchEngines.at(RULE_PROP_EXEC_PID)->match(w->getPID()))
                        match = true;
                }
                if (!match)
                    return false;
                break;
        }
    }

    return true;
}

std::expected<SP<CWindowRule>, std::string> CWindowRule::buildFromExecString(std::string&& s) {
    CVarList2       varlist(std::move(s), 0, ';');
    SP<CWindowRule> wr = makeShared<CWindowRule>("__exec_rule");

    for (const auto& el : varlist) {
        // split element by space, can't do better
        size_t spacePos = el.find(' ');
        if (spacePos != std::string::npos) {
            // great, split and try to parse
            auto       LHS    = el.substr(0, spacePos);
            const auto EFFECT = windowEffects()->get(LHS);

            if (!EFFECT.has_value() || *EFFECT == WINDOW_RULE_EFFECT_NONE)
                continue; // invalid...

            auto res = wr->addEffect(*EFFECT, std::string{el.substr(spacePos + 1)});
            if (!res)
                return std::unexpected(res.error());
            continue;
        }

        // assume 1 maybe...

        const auto EFFECT = windowEffects()->get(el);

        if (!EFFECT.has_value() || *EFFECT == WINDOW_RULE_EFFECT_NONE)
            continue; // invalid...

        auto res = wr->addEffect(*EFFECT, std::string{"1"});
        if (!res)
            return std::unexpected(res.error());
    }

    return wr;
}
