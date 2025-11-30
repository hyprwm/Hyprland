#include "WindowRuleEffectContainer.hpp"

using namespace Desktop;
using namespace Desktop::Rule;

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
    "position",               //
    "size",                   //
    "pseudo",                 //
    "monitor",                //
    "workspace",              //
    "no_initial_focus",       //
    "pin",                    //
    "group",                  //
    "suppress_event",         //
    "content",                //
    "no_close_for",           //
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
    "no_shortcuts_inhibit",   //
    "opaque",                 //
    "force_rgbx",             //
    "sync_fullscreen",        //
    "immediate",              //
    "xray",                   //
    "render_unfocused",       //
    "no_screen_share",        //
    "no_vrr",                 //
    "scroll_mouse",           //
    "scroll_touchpad",        //
    "stay_focused",           //
    "__internal_last_static", //
};

// This is here so that if we change the rules, we get reminded to update
// the strings.
static_assert(WINDOW_RULE_EFFECT_LAST_STATIC == 53);

CWindowRuleEffectContainer::CWindowRuleEffectContainer() : IEffectContainer<eWindowRuleEffect>(std::vector<std::string>{EFFECT_STRINGS}) {
    ;
}
