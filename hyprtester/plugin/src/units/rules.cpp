#include "../private.hpp"
#include "../globals.hpp"

static Desktop::Rule::CWindowRuleEffectContainer::storageType windowRuleIDX = 0;

static SDispatchResult                                        addWindowRule(lua_State* L) {
    windowRuleIDX = Desktop::Rule::windowEffects()->registerEffect("plugin_rule");

    if (Desktop::Rule::windowEffects()->registerEffect("plugin_rule") != windowRuleIDX)
        return {.success = false, .error = "re-registering returned a different id?"};
    return {};
}

static SDispatchResult checkWindowRule(lua_State* L) {
    const auto PLASTWINDOW = Desktop::focusState()->window();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window"};

    if (!PLASTWINDOW->m_ruleApplicator->m_otherProps.props.contains(windowRuleIDX))
        return {.success = false, .error = "No rule"};

    if (PLASTWINDOW->m_ruleApplicator->m_otherProps.props[windowRuleIDX]->effect != "effect")
        return {.success = false, .error = "Effect isn't \"effect\""};

    return {};
}

static Desktop::Rule::CLayerRuleEffectContainer::storageType layerRuleIDX = 0;

static SDispatchResult                                       addLayerRule(lua_State* L) {
    layerRuleIDX = Desktop::Rule::layerEffects()->registerEffect("plugin_rule");

    if (Desktop::Rule::layerEffects()->registerEffect("plugin_rule") != layerRuleIDX)
        return {.success = false, .error = "re-registering returned a different id?"};
    return {};
}

static SDispatchResult checkLayerRule(lua_State* L) {
    if (Desktop::layerState()->layers().size() != 3)
        return {.success = false, .error = "Layers under test not here"};

    for (const auto& layer : Desktop::layerState()->layers()) {
        if (layer->m_namespace == "rule-layer") {

            if (!layer->m_ruleApplicator->m_otherProps.props.contains(layerRuleIDX))
                return {.success = false, .error = "No rule"};

            if (layer->m_ruleApplicator->m_otherProps.props[layerRuleIDX]->effect != "effect")
                return {.success = false, .error = "Effect isn't \"effect\""};

        } else if (layer->m_namespace == "norule-layer") {

            if (layer->m_ruleApplicator->m_otherProps.props.contains(layerRuleIDX))
                return {.success = false, .error = "Rule even though it shouldn't"};

        } else
            return {.success = false, .error = "Unrecognized layer"};
    }

    return {};
}

REGISTER_UNIT(rules) {
    registerLuaFn<addWindowRule>("add_window_rule");
    registerLuaFn<checkWindowRule>("check_window_rule");
    registerLuaFn<addLayerRule>("add_layer_rule");
    registerLuaFn<checkLayerRule>("check_layer_rule");
}