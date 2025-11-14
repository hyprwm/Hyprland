#include "WindowRule.hpp"
#include "../../Window.hpp"
#include "../../../helpers/Monitor.hpp"
#include "../../../Compositor.hpp"
#include "../../../managers/TokenManager.hpp"

using namespace Desktop;
using namespace Desktop::Rule;

static const std::unordered_map<eWindowRuleEffect, std::string> EFFECT_STRINGS = {
    {WINDOW_RULE_EFFECT_FLOAT, "float"},                               //
    {WINDOW_RULE_EFFECT_TILE, "tile"},                                 //
    {WINDOW_RULE_EFFECT_FULLSCREEN, "fullscreen"},                     //
    {WINDOW_RULE_EFFECT_MAXIMIZE, "maximize"},                         //
    {WINDOW_RULE_EFFECT_FULLSCREENSTATE, "fullscreen_state"},          //
    {WINDOW_RULE_EFFECT_MOVE, "move"},                                 //
    {WINDOW_RULE_EFFECT_SIZE, "size"},                                 //
    {WINDOW_RULE_EFFECT_CENTER, "center"},                             //
    {WINDOW_RULE_EFFECT_PSEUDO, "pseudo"},                             //
    {WINDOW_RULE_EFFECT_MONITOR, "monitor"},                           //
    {WINDOW_RULE_EFFECT_WORKSPACE, "workspace"},                       //
    {WINDOW_RULE_EFFECT_NOINITIALFOCUS, "no_initial_focus"},           //
    {WINDOW_RULE_EFFECT_PIN, "pin"},                                   //
    {WINDOW_RULE_EFFECT_GROUP, "group"},                               //
    {WINDOW_RULE_EFFECT_SUPPRESSEVENT, "suppress_event"},              //
    {WINDOW_RULE_EFFECT_CONTENT, "content"},                           //
    {WINDOW_RULE_EFFECT_NOCLOSEFOR, "no_close_for"},                   //
    {WINDOW_RULE_EFFECT_ROUNDING, "rounding"},                         //
    {WINDOW_RULE_EFFECT_ROUNDING_POWER, "rounding_power"},             //
    {WINDOW_RULE_EFFECT_PERSISTENT_SIZE, "persistent_size"},           //
    {WINDOW_RULE_EFFECT_ANIMATION, "animation"},                       //
    {WINDOW_RULE_EFFECT_BORDER_COLOR, "border_color"},                 //
    {WINDOW_RULE_EFFECT_IDLE_INHIBIT, "idle_inhibit"},                 //
    {WINDOW_RULE_EFFECT_OPACITY, "opacity"},                           //
    {WINDOW_RULE_EFFECT_TAG, "tag"},                                   //
    {WINDOW_RULE_EFFECT_MAX_SIZE, "max_size"},                         //
    {WINDOW_RULE_EFFECT_MIN_SIZE, "min_size"},                         //
    {WINDOW_RULE_EFFECT_BORDER_SIZE, "border_size"},                   //
    {WINDOW_RULE_EFFECT_ALLOWS_INPUT, "allows_input"},                 //
    {WINDOW_RULE_EFFECT_DIM_AROUND, "dim_around"},                     //
    {WINDOW_RULE_EFFECT_DECORATE, "decorate"},                         //
    {WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE, "focus_on_activate"},       //
    {WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO, "keep_aspect_ratio"},       //
    {WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR, "nearest_neighbor"},         //
    {WINDOW_RULE_EFFECT_NO_ANIM, "no_anim"},                           //
    {WINDOW_RULE_EFFECT_NO_BLUR, "no_blur"},                           //
    {WINDOW_RULE_EFFECT_NO_DIM, "no_dim"},                             //
    {WINDOW_RULE_EFFECT_NO_FOCUS, "no_focus"},                         //
    {WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE, "no_follow_mouse"},           //
    {WINDOW_RULE_EFFECT_NO_MAX_SIZE, "no_max_size"},                   //
    {WINDOW_RULE_EFFECT_NO_SHADOW, "no_shadow"},                       //
    {WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT, "no_shortcuts_inhibit"}, //
    {WINDOW_RULE_EFFECT_OPAQUE, "opaque"},                             //
    {WINDOW_RULE_EFFECT_FORCE_RGBX, "force_rgbx"},                     //
    {WINDOW_RULE_EFFECT_SYNC_FULLSCREEN, "sync_fullscreen"},           //
    {WINDOW_RULE_EFFECT_IMMEDIATE, "immediate"},                       //
    {WINDOW_RULE_EFFECT_XRAY, "xray"},                                 //
    {WINDOW_RULE_EFFECT_RENDER_UNFOCUSED, "render_unfocused"},         //
    {WINDOW_RULE_EFFECT_NO_SCREEN_SHARE, "no_screen_share"},           //
    {WINDOW_RULE_EFFECT_NO_VRR, "no_vrr"},                             //
    {WINDOW_RULE_EFFECT_SCROLL_MOUSE, "scroll_mouse"},                 //
    {WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD, "scroll_touchpad"},           //
    {WINDOW_RULE_EFFECT_STAY_FOCUSED, "stay_focused"},                 //
};

std::optional<eWindowRuleEffect> Rule::matchWindowEffectFromString(const std::string_view& s) {
    const auto IT = std::ranges::find_if(EFFECT_STRINGS, [&s](const auto& el) { return el.second == s; });
    if (IT == EFFECT_STRINGS.end())
        return std::nullopt;

    return IT->first;
}

std::optional<eWindowRuleEffect> Rule::matchWindowEffectFromString(const std::string& s) {
    return matchWindowEffectFromString(std::string_view{s});
}

const std::vector<std::string>& Rule::allWindowEffectStrings() {
    static std::vector<std::string> strings;
    static bool                     once = true;
    if (once) {
        for (const auto& [k, v] : EFFECT_STRINGS) {
            strings.emplace_back(v);
        }
        once = false;
    }
    return strings;
}

std::optional<Vector2D> Rule::parseRelativeVector(PHLWINDOW w, const std::string& s) {
    try {
        const auto  VALUE    = s.substr(s.find(' ') + 1);
        const auto  SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
        const auto  SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

        const auto  MAXSIZE = w->requestedMaxSize();

        const float SIZEX = SIZEXSTR == "max" ? std::clamp(MAXSIZE.x, MIN_WINDOW_SIZE, g_pCompositor->m_lastMonitor->m_size.x) :
                                                stringToPercentage(SIZEXSTR, g_pCompositor->m_lastMonitor->m_size.x);

        const float SIZEY = SIZEYSTR == "max" ? std::clamp(MAXSIZE.y, MIN_WINDOW_SIZE, g_pCompositor->m_lastMonitor->m_size.y) :
                                                stringToPercentage(SIZEYSTR, g_pCompositor->m_lastMonitor->m_size.y);

        return Vector2D{SIZEX, SIZEY};

    } catch (...) { Debug::log(LOG, "Rule size failed, rule: {}", s); }

    return std::nullopt;
}

CWindowRule::CWindowRule(const std::string& name) : IRule(name) {
    ;
}

eRuleType CWindowRule::type() {
    return RULE_TYPE_WINDOW;
}

void CWindowRule::addEffect(eWindowRuleEffect e, const std::string& result) {
    m_effects.emplace_back(std::make_pair<>(e, result));
    m_effectSet.emplace(e);
}

const std::vector<std::pair<eWindowRuleEffect, std::string>>& CWindowRule::effects() {
    return m_effects;
}

bool CWindowRule::matches(PHLWINDOW w, bool allowEnvLookup) {
    if (m_matchEngines.empty())
        return false;

    for (const auto& [prop, engine] : m_matchEngines) {
        switch (prop) {
            default: {
                Debug::log(TRACE, "CWindowRule::matches: skipping prop entry {}", sc<std::underlying_type_t<eRuleProperty>>(prop));
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
                if (!engine->match(g_pCompositor->m_lastWindow == w))
                    return false;
                break;
            case RULE_PROP_GROUP:
                if (!engine->match(w->m_groupData.pNextWindow))
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
                if (!engine->match(NContentType::toString(w->getContentType())))
                    return false;
                break;
            case RULE_PROP_XDG_TAG:
                if (w->xdgTag().has_value() && !engine->match(*w->xdgTag()))
                    return false;
                break;
            case RULE_PROP_EXEC_TOKEN:
                // this is only allowed on static rules, we don't need it on dynamic plus it's expensive
                if (!allowEnvLookup)
                    break;

                const auto ENV = w->getEnv();
                if (ENV.contains(EXEC_RULE_ENV_NAME)) {
                    const auto TKN = ENV.at(EXEC_RULE_ENV_NAME);
                    if (!engine->match(TKN))
                        return false;
                    break;
                }

                return false;
        }
    }

    return true;
}

SP<CWindowRule> CWindowRule::buildFromExecString(std::string&& s) {
    CVarList2       varlist(std::move(s), 0, ';');
    SP<CWindowRule> wr = makeShared<CWindowRule>("__exec_rule");

    for (const auto& el : varlist) {
        // split element by space, can't do better
        size_t spacePos = el.find(' ');
        if (spacePos != std::string::npos) {
            // great, split and try to parse
            auto       LHS    = el.substr(0, spacePos);
            const auto EFFECT = matchWindowEffectFromString(LHS);

            if (!EFFECT.has_value() || *EFFECT == WINDOW_RULE_EFFECT_NONE)
                continue; // invalid...

            wr->addEffect(*EFFECT, std::string{el.substr(spacePos + 1)});
            continue;
        }

        // assume 1 maybe...

        const auto EFFECT = matchWindowEffectFromString(el);

        if (!EFFECT.has_value() || *EFFECT == WINDOW_RULE_EFFECT_NONE)
            continue; // invalid...

        wr->addEffect(*EFFECT, std::string{"1"});
    }

    const auto TOKEN = g_pTokenManager->registerNewToken(nullptr, std::chrono::seconds(1));

    wr->markAsExecRule(TOKEN, false /* TODO: could be nice. */);
    wr->registerMatch(RULE_PROP_EXEC_TOKEN, TOKEN);

    return wr;
}

const std::unordered_set<eWindowRuleEffect>& CWindowRule::effectsSet() {
    return m_effectSet;
}
