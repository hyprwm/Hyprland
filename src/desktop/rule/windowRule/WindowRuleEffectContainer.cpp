#include "WindowRuleEffectContainer.hpp"
#include "../../../config/values/types/BoolValue.hpp"
#include "../../../config/values/types/IntValue.hpp"
#include "../../../config/values/types/FloatValue.hpp"
#include "../../../config/values/types/StringValue.hpp"
#include "../../../config/values/types/GradientValue.hpp"
#include "../../../config/values/types/ExpressionVec2Value.hpp"

using namespace Desktop;
using namespace Desktop::Rule;
using namespace Config::Values;

//
SP<CWindowRuleEffectContainer> Rule::windowEffects() {
    static SP<CWindowRuleEffectContainer> container = makeShared<CWindowRuleEffectContainer>();
    return container;
}

static const std::vector<std::string> EFFECT_STRINGS = {
    "__internal_none",        //
    "float",                  //
    "tile",                   //
    "fullscreen",             //
    "maximize",               //
    "fullscreen_state",       //
    "move",                   //
    "size",                   //
    "center",                 //
    "pseudo",                 //
    "monitor",                //
    "workspace",              //
    "no_initial_focus",       //
    "pin",                    //
    "group",                  //
    "suppress_event",         //
    "content",                //
    "no_close_for",           //
    "scrolling_width",        //
    "rounding",               //
    "rounding_power",         //
    "persistent_size",        //
    "animation",              //
    "border_color",           //
    "idle_inhibit",           //
    "opacity",                //
    "tag",                    //
    "max_size",               //
    "min_size",               //
    "border_size",            //
    "allows_input",           //
    "dim_around",             //
    "decorate",               //
    "focus_on_activate",      //
    "keep_aspect_ratio",      //
    "nearest_neighbor",       //
    "no_anim",                //
    "no_blur",                //
    "no_dim",                 //
    "no_focus",               //
    "no_follow_mouse",        //
    "no_max_size",            //
    "no_shadow",              //
    "no_glow",                //
    "no_wobble",              //
    "no_shortcuts_inhibit",   //
    "opaque",                 //
    "force_rgbx",             //
    "sync_fullscreen",        //
    "immediate",              //
    "xray",                   //
    "render_unfocused",       //
    "no_screen_share",        //
    "no_vrr",                 //
    "no_auto_hdr",            //
    "tonemap",                //
    "scroll_mouse",           //
    "scroll_touchpad",        //
    "stay_focused",           //
    "confine_pointer",        //
    "no_xdg_drags",           //
    "__internal_last_static", //
};

// This is here so that if we change the rules, we get reminded to update
// the strings.
static_assert(WINDOW_RULE_EFFECT_LAST_STATIC == 61);

static SP<Config::Values::IValue> boolValue(const char* name, Config::BOOL def = false) {
    return makeShared<CBoolValue>(name, "", def);
}

static SP<Config::Values::IValue> intValue(const char* name, Config::INTEGER def, std::optional<Config::INTEGER> min = {}, std::optional<Config::INTEGER> max = {}) {
    return makeShared<CIntValue>(name, "", def, SIntValueOptions{.min = min, .max = max});
}

static SP<Config::Values::IValue> floatValue(const char* name, Config::FLOAT def, std::optional<Config::FLOAT> min = {}, std::optional<Config::FLOAT> max = {}) {
    return makeShared<CFloatValue>(name, "", def, SFloatValueOptions{.min = min, .max = max});
}

static SP<Config::Values::IValue> stringValue(const char* name) {
    return makeShared<CStringValue>(name, "", Config::STRING{});
}

static SP<Config::Values::IValue> expressionValue(const char* name) {
    return makeShared<CExpressionVec2Value>(name, "", Math::SExpressionVec2{});
}

static SP<Config::Values::IValue> gradientValue(const char* name) {
    return makeShared<CGradientValue>(name, "", CHyprColor(0xFF000000));
}

CWindowRuleEffectContainer::CWindowRuleEffectContainer() : IEffectContainer<eWindowRuleEffect>(std::vector<std::string>{EFFECT_STRINGS}) {
    registerStaticEffect(WINDOW_RULE_EFFECT_FLOAT, boolValue("float"));
    registerStaticEffect(WINDOW_RULE_EFFECT_TILE, boolValue("tile"));
    registerStaticEffect(WINDOW_RULE_EFFECT_FULLSCREEN, boolValue("fullscreen"));
    registerStaticEffect(WINDOW_RULE_EFFECT_MAXIMIZE, boolValue("maximize"));
    registerStaticEffect(WINDOW_RULE_EFFECT_FULLSCREENSTATE, stringValue("fullscreen_state"));
    registerStaticEffect(WINDOW_RULE_EFFECT_MOVE, expressionValue("move"));
    registerStaticEffect(WINDOW_RULE_EFFECT_SIZE, expressionValue("size"));
    registerStaticEffect(WINDOW_RULE_EFFECT_CENTER, boolValue("center"));
    registerStaticEffect(WINDOW_RULE_EFFECT_PSEUDO, boolValue("pseudo"));
    registerStaticEffect(WINDOW_RULE_EFFECT_MONITOR, stringValue("monitor"));
    registerStaticEffect(WINDOW_RULE_EFFECT_WORKSPACE, stringValue("workspace"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NOINITIALFOCUS, boolValue("no_initial_focus"));
    registerStaticEffect(WINDOW_RULE_EFFECT_PIN, boolValue("pin"));
    registerStaticEffect(WINDOW_RULE_EFFECT_GROUP, stringValue("group"));
    registerStaticEffect(WINDOW_RULE_EFFECT_SUPPRESSEVENT, stringValue("suppress_event"));
    registerStaticEffect(WINDOW_RULE_EFFECT_CONTENT, stringValue("content"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NOCLOSEFOR, intValue("no_close_for", 0));
    registerStaticEffect(WINDOW_RULE_EFFECT_SCROLLING_WIDTH, floatValue("scrolling_width", 0.F));
    registerStaticEffect(WINDOW_RULE_EFFECT_ROUNDING, intValue("rounding", 0, 0, 100));
    registerStaticEffect(WINDOW_RULE_EFFECT_ROUNDING_POWER, floatValue("rounding_power", 2.F, 1.F, 10.F));
    registerStaticEffect(WINDOW_RULE_EFFECT_PERSISTENT_SIZE, boolValue("persistent_size"));
    registerStaticEffect(WINDOW_RULE_EFFECT_ANIMATION, stringValue("animation"));
    registerStaticEffect(WINDOW_RULE_EFFECT_BORDER_COLOR, gradientValue("border_color"));
    registerStaticEffect(WINDOW_RULE_EFFECT_IDLE_INHIBIT, stringValue("idle_inhibit"));
    registerStaticEffect(WINDOW_RULE_EFFECT_OPACITY, stringValue("opacity"));
    registerStaticEffect(WINDOW_RULE_EFFECT_TAG, stringValue("tag"));
    registerStaticEffect(WINDOW_RULE_EFFECT_MAX_SIZE, expressionValue("max_size"));
    registerStaticEffect(WINDOW_RULE_EFFECT_MIN_SIZE, expressionValue("min_size"));
    registerStaticEffect(WINDOW_RULE_EFFECT_BORDER_SIZE, intValue("border_size", 0));
    registerStaticEffect(WINDOW_RULE_EFFECT_ALLOWS_INPUT, boolValue("allows_input"));
    registerStaticEffect(WINDOW_RULE_EFFECT_DIM_AROUND, boolValue("dim_around"));
    registerStaticEffect(WINDOW_RULE_EFFECT_DECORATE, boolValue("decorate", true));
    registerStaticEffect(WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE, boolValue("focus_on_activate"));
    registerStaticEffect(WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO, boolValue("keep_aspect_ratio"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR, boolValue("nearest_neighbor"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_ANIM, boolValue("no_anim"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_BLUR, boolValue("no_blur"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_DIM, boolValue("no_dim"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_FOCUS, boolValue("no_focus"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE, boolValue("no_follow_mouse"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_MAX_SIZE, boolValue("no_max_size"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_SHADOW, boolValue("no_shadow"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_GLOW, boolValue("no_glow"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_WOBBLE, boolValue("no_wobble"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT, boolValue("no_shortcuts_inhibit"));
    registerStaticEffect(WINDOW_RULE_EFFECT_OPAQUE, boolValue("opaque"));
    registerStaticEffect(WINDOW_RULE_EFFECT_FORCE_RGBX, boolValue("force_rgbx"));
    registerStaticEffect(WINDOW_RULE_EFFECT_SYNC_FULLSCREEN, boolValue("sync_fullscreen"));
    registerStaticEffect(WINDOW_RULE_EFFECT_IMMEDIATE, boolValue("immediate"));
    registerStaticEffect(WINDOW_RULE_EFFECT_XRAY, boolValue("xray"));
    registerStaticEffect(WINDOW_RULE_EFFECT_RENDER_UNFOCUSED, boolValue("render_unfocused"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_SCREEN_SHARE, boolValue("no_screen_share"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_VRR, boolValue("no_vrr"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_AUTO_HDR, boolValue("no_auto_hdr"));
    registerStaticEffect(WINDOW_RULE_EFFECT_TONEMAP, stringValue("tonemap"));
    registerStaticEffect(WINDOW_RULE_EFFECT_SCROLL_MOUSE, floatValue("scroll_mouse", 1.F, 0.01F, 10.F));
    registerStaticEffect(WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD, floatValue("scroll_touchpad", 1.F, 0.01F, 10.F));
    registerStaticEffect(WINDOW_RULE_EFFECT_STAY_FOCUSED, boolValue("stay_focused"));
    registerStaticEffect(WINDOW_RULE_EFFECT_CONFINE_POINTER, boolValue("confine_pointer"));
    registerStaticEffect(WINDOW_RULE_EFFECT_NO_XDG_DRAGS, boolValue("no_xdg_drags"));
}
