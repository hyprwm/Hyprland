#include "LayerRule.hpp"
#include "../../../debug/log/Logger.hpp"
#include "../../../helpers/MiscFunctions.hpp"
#include "../../view/LayerSurface.hpp"

#include <algorithm>
#include <format>
#include <hyprutils/string/Numeric.hpp>

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

static std::expected<int64_t, std::string> parseAboveLock(const std::string& raw) {
    auto parsed = strToNumber<int64_t>(raw);
    if (!parsed)
        return std::unexpected(std::format("above_lock rule \"{}\" failed with: {}", raw, numericParseError(parsed.error())));

    return std::clamp(*parsed, int64_t{0}, int64_t{2});
}

static std::expected<LayerRuleEffectValue, std::string> parseLayerRuleEffect(CLayerRuleEffectContainer::storageType e, const std::string& raw) {
    if (layerEffects()->isEffectDynamic(e))
        return std::string{raw};

    const auto EFFECT_NAME = layerEffects()->get(e);

    switch (e) {
        default: return std::unexpected(std::format("unknown layer rule effect {}", e));

        case LAYER_RULE_EFFECT_NONE: return std::monostate{};

        case LAYER_RULE_EFFECT_NO_ANIM:
        case LAYER_RULE_EFFECT_BLUR:
        case LAYER_RULE_EFFECT_BLUR_POPUPS:
        case LAYER_RULE_EFFECT_DIM_AROUND:
        case LAYER_RULE_EFFECT_XRAY:
        case LAYER_RULE_EFFECT_NO_SCREEN_SHARE: return truthy(raw);

        case LAYER_RULE_EFFECT_ORDER: {
            auto parsed = parseInt(EFFECT_NAME, raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return *parsed;
        }
        case LAYER_RULE_EFFECT_ABOVE_LOCK: {
            auto parsed = parseAboveLock(raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return *parsed;
        }
        case LAYER_RULE_EFFECT_IGNORE_ALPHA: {
            auto parsed = parseFloat(EFFECT_NAME, raw);
            if (!parsed)
                return std::unexpected(parsed.error());
            return std::clamp(*parsed, 0.F, 1.F);
        }
        case LAYER_RULE_EFFECT_ANIMATION: return std::string{raw};
    }
}

CLayerRule::CLayerRule(const std::string& name) : IRule(name) {
    ;
}

eRuleType CLayerRule::type() {
    return RULE_TYPE_LAYER;
}

std::expected<void, std::string> CLayerRule::addEffect(CLayerRule::storageType e, const std::string& result) {
    auto parsed = parseLayerRuleEffect(e, result);
    if (!parsed)
        return std::unexpected(parsed.error());

    m_effects.emplace_back(SLayerRuleEffect{.key = e, .raw = result, .value = std::move(*parsed)});

    return {};
}

const std::vector<SLayerRuleEffect>& CLayerRule::effects() {
    return m_effects;
}

bool CLayerRule::matches(PHLLS ls) {
    if (m_matchEngines.empty() || !m_enabled)
        return false;

    for (const auto& [prop, engine] : m_matchEngines) {
        switch (prop) {
            default: {
                Log::logger->log(Log::TRACE, "CLayerRule::matches: skipping prop entry {}", sc<std::underlying_type_t<eRuleProperty>>(prop));
                break;
            }

            case RULE_PROP_NAMESPACE:
                if (!engine->match(ls->m_namespace))
                    return false;
                break;
        }
    }

    return true;
}

void CLayerRule::setEnabled(bool enable) {
    m_enabled = enable;
}

bool CLayerRule::isEnabled() const {
    return m_enabled;
}
