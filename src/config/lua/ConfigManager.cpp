#include "ConfigManager.hpp"
#include "LuaBindings.hpp"

#include <climits>
#include <functional>

#include "types/LuaConfigBool.hpp"
#include "types/LuaConfigCssGap.hpp"
#include "types/LuaConfigFloat.hpp"
#include "types/LuaConfigFontWeight.hpp"
#include "types/LuaConfigGradient.hpp"
#include "types/LuaConfigInt.hpp"
#include "types/LuaConfigString.hpp"
#include "types/LuaConfigVec2.hpp"
#include "types/LuaConfigColor.hpp"

#include "../supplementary/jeremy/Jeremy.hpp"
#include "../shared/workspace/WorkspaceRuleManager.hpp"
#include "../shared/monitor/MonitorRuleManager.hpp"
#include "../shared/monitor/Parser.hpp"
#include "../shared/animation/AnimationTree.hpp"
#include "../shared/inotify/ConfigWatcher.hpp"

#include "../../desktop/rule/Engine.hpp"
#include "../../desktop/rule/windowRule/WindowRule.hpp"
#include "../../desktop/rule/windowRule/WindowRuleEffectContainer.hpp"
#include "../../desktop/rule/layerRule/LayerRule.hpp"
#include "../../desktop/rule/layerRule/LayerRuleEffectContainer.hpp"
#include "../../helpers/MiscFunctions.hpp"

#include "../../event/EventBus.hpp"
#include "../../Compositor.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/supplementary/WorkspaceAlgoMatcher.hpp"
#include "../../render/Renderer.hpp"
#include "../../hyprerror/HyprError.hpp"
#include "../../xwayland/XWayland.hpp"
#include "../../managers/EventManager.hpp"
#include "../../debug/HyprNotificationOverlay.hpp"
#include "../../render/decorations/CHyprGroupBarDecoration.hpp"

using namespace Config;
using namespace Config::Lua;

namespace {
    struct SFieldDesc {
        const char*                       name;
        std::function<ILuaConfigValue*()> factory;
    };

    struct SMonitorFieldDesc {
        const char*                                                name;
        std::function<ILuaConfigValue*()>                          factory;
        std::function<bool(ILuaConfigValue*, CMonitorRuleParser&)> apply;
    };

    inline const SMonitorFieldDesc MONITOR_FIELDS[] = {
        {"mode", []() -> ILuaConfigValue* { return new CLuaConfigString("preferred"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parseMode(*static_cast<const Config::STRING*>(v->data())); }},
        {"position", []() -> ILuaConfigValue* { return new CLuaConfigString("auto"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parsePosition(*static_cast<const Config::STRING*>(v->data())); }},
        {"scale", []() -> ILuaConfigValue* { return new CLuaConfigString("auto"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parseScale(*static_cast<const Config::STRING*>(v->data())); }},
        {"disabled", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             if (*static_cast<const Config::BOOL*>(v->data()))
                 p.setDisabled();
             return true;
         }},
        {"transform", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 7); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_transform = static_cast<wl_output_transform>(*static_cast<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"mirror", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.setMirror(*static_cast<const Config::STRING*>(v->data()));
             return true;
         }},
        {"bitdepth", []() -> ILuaConfigValue* { return new CLuaConfigInt(8); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_enable10bit = *static_cast<const Config::INTEGER*>(v->data()) == 10;
             return true;
         }},
        {"cm", []() -> ILuaConfigValue* { return new CLuaConfigString("srgb"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parseCM(*static_cast<const Config::STRING*>(v->data())); }},
        {"sdr_eotf", []() -> ILuaConfigValue* { return new CLuaConfigString("default"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrEotf = NTransferFunction::fromString(*static_cast<const Config::STRING*>(v->data()));
             return true;
         }},
        {"sdrbrightness", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrBrightness = *static_cast<const Config::FLOAT*>(v->data());
             return true;
         }},
        {"sdrsaturation", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrSaturation = *static_cast<const Config::FLOAT*>(v->data());
             return true;
         }},
        {"vrr", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 3); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_vrr = static_cast<int>(*static_cast<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"icc", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parseICC(*static_cast<const Config::STRING*>(v->data())); }},
        {"supports_wide_color", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, -1, 1); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_supportsWideColor = static_cast<int>(*static_cast<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"supports_hdr", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, -1, 1); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_supportsHDR = static_cast<int>(*static_cast<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"sdr_min_luminance", []() -> ILuaConfigValue* { return new CLuaConfigFloat(0.2F); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrMinLuminance = *static_cast<const Config::FLOAT*>(v->data());
             return true;
         }},
        {"sdr_max_luminance", []() -> ILuaConfigValue* { return new CLuaConfigInt(80); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrMaxLuminance = static_cast<int>(*static_cast<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"min_luminance", []() -> ILuaConfigValue* { return new CLuaConfigFloat(-1.F); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_minLuminance = *static_cast<const Config::FLOAT*>(v->data());
             return true;
         }},
        {"max_luminance", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_maxLuminance = static_cast<int>(*static_cast<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"max_avg_luminance", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_maxAvgLuminance = static_cast<int>(*static_cast<const Config::INTEGER*>(v->data()));
             return true;
         }},
    };

    // storageType for both window and layer rule effects resolves to uint16_t
    // (eWindowRuleEffect and eLayerRuleEffect are uint8_t, promoted to uint16_t by IEffectContainer)
    struct SWindowRuleEffectDesc {
        const char*                       name;
        std::function<ILuaConfigValue*()> factory;
        uint16_t                          effect;
    };

    struct SLayerRuleEffectDesc {
        const char*                       name;
        std::function<ILuaConfigValue*()> factory;
        uint16_t                          effect;
        bool                              invert = false;
    };

    struct SWorkspaceRuleFieldDesc {
        const char*                                                    name;
        std::function<ILuaConfigValue*()>                              factory;
        std::function<void(ILuaConfigValue*, Config::CWorkspaceRule&)> apply;
    };

    using WE = Desktop::Rule::eWindowRuleEffect;
    using LE = Desktop::Rule::eLayerRuleEffect;

    inline const SWindowRuleEffectDesc WINDOW_RULE_EFFECT_DESCS[] = {
        // static bool effects
        {"float", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_FLOAT},
        {"tile", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_TILE},
        {"fullscreen", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_FULLSCREEN},
        {"maximize", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_MAXIMIZE},
        {"center", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_CENTER},
        {"pseudo", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_PSEUDO},
        {"no_initial_focus", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NOINITIALFOCUS},
        {"pin", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_PIN},
        // static string effects
        {"fullscreen_state", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_FULLSCREENSTATE},
        {"move", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_MOVE},
        {"size", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_SIZE},
        {"monitor", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_MONITOR},
        {"workspace", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_WORKSPACE},
        {"group", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_GROUP},
        {"suppress_event", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_SUPPRESSEVENT},
        {"content", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_CONTENT},
        // static int/float effects
        {"no_close_for", []() -> ILuaConfigValue* { return new CLuaConfigInt(0); }, WE::WINDOW_RULE_EFFECT_NOCLOSEFOR},
        {"scrolling_width", []() -> ILuaConfigValue* { return new CLuaConfigFloat(0.F); }, WE::WINDOW_RULE_EFFECT_SCROLLING_WIDTH},
        // dynamic int effects
        {"rounding", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 20); }, WE::WINDOW_RULE_EFFECT_ROUNDING},
        {"border_size", []() -> ILuaConfigValue* { return new CLuaConfigInt(0); }, WE::WINDOW_RULE_EFFECT_BORDER_SIZE},
        // dynamic float effects
        {"rounding_power", []() -> ILuaConfigValue* { return new CLuaConfigFloat(2.F, 1.F, 10.F); }, WE::WINDOW_RULE_EFFECT_ROUNDING_POWER},
        {"scroll_mouse", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F, 0.01F, 10.F); }, WE::WINDOW_RULE_EFFECT_SCROLL_MOUSE},
        {"scroll_touchpad", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F, 0.01F, 10.F); }, WE::WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD},
        // dynamic string effects
        {"animation", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_ANIMATION},
        {"idle_inhibit", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_IDLE_INHIBIT},
        {"opacity", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_OPACITY},
        {"tag", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_TAG},
        // dynamic vec2 effects ("x y" format)
        {"max_size", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }, WE::WINDOW_RULE_EFFECT_MAX_SIZE},
        {"min_size", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }, WE::WINDOW_RULE_EFFECT_MIN_SIZE},
        // dynamic gradient effect
        {"border_color", []() -> ILuaConfigValue* { return new CLuaConfigGradient(CHyprColor(0xFF000000)); }, WE::WINDOW_RULE_EFFECT_BORDER_COLOR},
        // dynamic bool effects
        {"persistent_size", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_PERSISTENT_SIZE},
        {"allows_input", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_ALLOWS_INPUT},
        {"dim_around", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_DIM_AROUND},
        {"decorate", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }, WE::WINDOW_RULE_EFFECT_DECORATE},
        {"focus_on_activate", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE},
        {"keep_aspect_ratio", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO},
        {"nearest_neighbor", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR},
        {"no_anim", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_ANIM},
        {"no_blur", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_BLUR},
        {"no_dim", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_DIM},
        {"no_focus", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_FOCUS},
        {"no_follow_mouse", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE},
        {"no_max_size", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_MAX_SIZE},
        {"no_shadow", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_SHADOW},
        {"no_shortcuts_inhibit", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT},
        {"opaque", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_OPAQUE},
        {"force_rgbx", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_FORCE_RGBX},
        {"sync_fullscreen", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_SYNC_FULLSCREEN},
        {"immediate", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_IMMEDIATE},
        {"xray", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_XRAY},
        {"render_unfocused", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_RENDER_UNFOCUSED},
        {"no_screen_share", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_SCREEN_SHARE},
        {"no_vrr", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_VRR},
        {"stay_focused", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_STAY_FOCUSED},
    };

    inline const SLayerRuleEffectDesc LAYER_RULE_EFFECT_DESCS[] = {
        {"no_anim", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_NO_ANIM},
        // blur = true enables blur; no_blur = true disables it (invert flag)
        {"blur", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_BLUR},
        {"no_blur", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_BLUR, true},
        {"blur_popups", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_BLUR_POPUPS},
        {"ignore_alpha", []() -> ILuaConfigValue* { return new CLuaConfigFloat(0.F, 0.F, 1.F); }, LE::LAYER_RULE_EFFECT_IGNORE_ALPHA},
        {"dim_around", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_DIM_AROUND},
        {"xray", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_XRAY},
        {"animation", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, LE::LAYER_RULE_EFFECT_ANIMATION},
        {"order", []() -> ILuaConfigValue* { return new CLuaConfigInt(0); }, LE::LAYER_RULE_EFFECT_ORDER},
        {"above_lock", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 2); }, LE::LAYER_RULE_EFFECT_ABOVE_LOCK},
        {"no_screen_share", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_NO_SCREEN_SHARE},
    };

    inline const SWorkspaceRuleFieldDesc WORKSPACE_RULE_FIELDS[] = {
        {"monitor", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_monitor = *static_cast<const Config::STRING*>(v->data()); }},
        {"default", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_isDefault = *static_cast<const Config::BOOL*>(v->data()); }},
        {"persistent", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_isPersistent = *static_cast<const Config::BOOL*>(v->data()); }},
        {"gaps_in", []() -> ILuaConfigValue* { return new CLuaConfigCssGap(5); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_gapsIn = *static_cast<const Config::CCssGapData*>(v->data()); }},
        {"gaps_out", []() -> ILuaConfigValue* { return new CLuaConfigCssGap(20); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_gapsOut = *static_cast<const Config::CCssGapData*>(v->data()); }},
        {"float_gaps", []() -> ILuaConfigValue* { return new CLuaConfigCssGap(0); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_floatGaps = *static_cast<const Config::CCssGapData*>(v->data()); }},
        {"border_size", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_borderSize = *static_cast<const Config::INTEGER*>(v->data()); }},
        {"no_border", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_noBorder = *static_cast<const Config::BOOL*>(v->data()); }},
        {"no_rounding", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_noRounding = *static_cast<const Config::BOOL*>(v->data()); }},
        {"decorate", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_decorate = *static_cast<const Config::BOOL*>(v->data()); }},
        {"no_shadow", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_noShadow = *static_cast<const Config::BOOL*>(v->data()); }},
        {"on_created_empty", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_onCreatedEmptyRunCmd = *static_cast<const Config::STRING*>(v->data()); }},
        {"default_name", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_defaultName = *static_cast<const Config::STRING*>(v->data()); }},
        {"layout", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_layout = *static_cast<const Config::STRING*>(v->data()); }},
        {"animation", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_animationStyle = *static_cast<const Config::STRING*>(v->data()); }},
    };

    inline const SFieldDesc DEVICE_FIELDS[] = {
        {"sensitivity", []() -> ILuaConfigValue* { return new CLuaConfigFloat(0.F, -1.F, 1.F); }},
        {"accel_profile", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"rotation", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 359); }},
        {"kb_file", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"kb_layout", []() -> ILuaConfigValue* { return new CLuaConfigString("us"); }},
        {"kb_variant", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"kb_options", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"kb_rules", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"kb_model", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"repeat_rate", []() -> ILuaConfigValue* { return new CLuaConfigInt(25, 0, 200); }},
        {"repeat_delay", []() -> ILuaConfigValue* { return new CLuaConfigInt(600, 0, 2000); }},
        {"natural_scroll", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"tap_button_map", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"numlock_by_default", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"resolve_binds_by_sym", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"disable_while_typing", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},
        {"clickfinger_behavior", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"middle_button_emulation", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"tap-to-click", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},
        {"tap-and-drag", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},
        {"drag_lock", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 2); }},
        {"left_handed", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"scroll_method", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"scroll_button", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 300); }},
        {"scroll_button_lock", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"scroll_points", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"scroll_factor", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F, 0.F, 2.F); }},
        {"transform", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); }},
        {"output", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"enabled", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},
        {"region_position", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }},
        {"absolute_region_position", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"region_size", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }},
        {"relative_input", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"active_area_position", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }},
        {"active_area_size", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }},
        {"flip_x", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"flip_y", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"drag_3fg", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 2); }},
        {"keybinds", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},
        {"share_states", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 2); }},
        {"release_pressed_on_close", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
    };
}

CConfigManager::CConfigManager() : m_mainConfigPath(Supplementary::Jeremy::getMainConfigPath()->path) {
    ;
}

eConfigManagerType CConfigManager::type() {
    return CONFIG_LUA;
}

void CConfigManager::registerValue(const char* name, ILuaConfigValue* val) {
    m_configValues.emplace(name, UP<ILuaConfigValue>(val));
}

void CConfigManager::registerConfigTable() {
    // register hl.config({...})
    lua_newtable(m_lua);

    lua_pushlightuserdata(m_lua, this);
    lua_pushcclosure(
        m_lua,
        [](lua_State* L) -> int {
            auto* self = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

            if (!lua_istable(L, 1)) {
                self->addError("hl.config: argument must be a table");
                return 0;
            }

            // grab the call-site location once for all errors in this call
            lua_Debug   ar         = {};
            std::string sourceInfo = "?:?";
            if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
                const char* src = ar.source;
                if (src && src[0] == '@')
                    src++;
                sourceInfo = std::format("{}:{}", src ? src : "?", ar.currentline);
            }

            std::function<void(const std::string&, int)> walk = [&](const std::string& prefix, int tableIdx) {
                lua_pushnil(L);
                while (lua_next(L, tableIdx) != 0) {
                    // key at -2, value at -1
                    if (lua_type(L, -2) != LUA_TSTRING) {
                        lua_pop(L, 1);
                        continue;
                    }

                    const std::string key = lua_tostring(L, -2);
                    std::string       fullKey;
                    if (!prefix.empty()) {
                        fullKey.reserve(prefix.size() + 1 + key.size());
                        fullKey = prefix;
                        fullKey += '.';
                    }
                    fullKey += key;

                    // look up first
                    auto it = self->m_configValues.find(fullKey);

                    if (it == self->m_configValues.end() && lua_istable(L, -1))
                        walk(fullKey, lua_gettop(L));
                    else {
                        if (it == self->m_configValues.end())
                            self->m_errors.emplace_back(std::format("{}: unknown config key '{}'", sourceInfo, fullKey));
                        else {
                            const auto err = it->second->parse(L);
                            if (err.errorCode != PARSE_ERROR_OK)
                                self->m_errors.emplace_back(std::format("{}: error setting '{}': {}", sourceInfo, it->first, err.message));
                        }
                    }

                    lua_pop(L, 1); // pop value, keep key for lua_next
                }
            };

            walk("", 1);
            return 0;
        },
        1);
    lua_setfield(m_lua, -2, "config");

    // hl.device({name = "...", sensitivity = 0.5, ...})
    lua_pushlightuserdata(m_lua, this);
    lua_pushcclosure(
        m_lua,
        [](lua_State* L) -> int {
            auto* self = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

            if (!lua_istable(L, 1)) {
                self->addError("hl.device: argument must be a table");
                return 0;
            }

            lua_Debug   ar         = {};
            std::string sourceInfo = "?:?";
            if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
                const char* src = ar.source;
                if (src && src[0] == '@')
                    src++;
                sourceInfo = std::format("{}:{}", src ? src : "?", ar.currentline);
            }

            lua_getfield(L, 1, "name");
            if (!lua_isstring(L, -1)) {
                self->addError(std::format("{}: hl.device: 'name' field is required and must be a string", sourceInfo));
                lua_pop(L, 1);
                return 0;
            }
            std::string devName = lua_tostring(L, -1);
            lua_pop(L, 1);
            std::ranges::replace(devName, ' ', '-');

            lua_pushnil(L);
            while (lua_next(L, 1) != 0) {
                if (lua_type(L, -2) != LUA_TSTRING) {
                    lua_pop(L, 1);
                    continue;
                }

                const char* key = lua_tostring(L, -2);

                if (std::string_view{key} == "name") {
                    lua_pop(L, 1);
                    continue;
                }

                const SFieldDesc* desc = nullptr;
                for (const auto& fd : DEVICE_FIELDS) {
                    if (std::string_view{fd.name} == std::string_view{key}) {
                        desc = &fd;
                        break;
                    }
                }

                if (!desc) {
                    self->addError(std::format("{}: hl.device: unknown field '{}'", sourceInfo, key));
                    lua_pop(L, 1);
                    continue;
                }

                auto val = UP<ILuaConfigValue>(desc->factory());
                auto err = val->parse(L);
                if (err.errorCode != PARSE_ERROR_OK)
                    self->addError(std::format("{}: hl.device: field '{}': {}", sourceInfo, key, err.message));
                else
                    self->m_deviceConfigs[devName].values.insert_or_assign(key, std::move(val));

                lua_pop(L, 1);
            }

            return 0;
        },
        1);
    lua_setfield(m_lua, -2, "device");

    // hl.monitor({output = "", mode = "preferred", position = "auto", scale = "1", ...})
    lua_pushlightuserdata(m_lua, this);
    lua_pushcclosure(
        m_lua,
        [](lua_State* L) -> int {
            auto* self = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

            if (!lua_istable(L, 1)) {
                self->addError("hl.monitor: argument must be a table");
                return 0;
            }

            lua_Debug   ar         = {};
            std::string sourceInfo = "?:?";
            if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
                const char* src = ar.source;
                if (src && src[0] == '@')
                    src++;
                sourceInfo = std::format("{}:{}", src ? src : "?", ar.currentline);
            }

            lua_getfield(L, 1, "output");
            if (!lua_isstring(L, -1)) {
                self->addError(std::format("{}: hl.monitor: 'output' field is required and must be a string", sourceInfo));
                lua_pop(L, 1);
                return 0;
            }
            const std::string output = lua_tostring(L, -1);
            lua_pop(L, 1);

            CMonitorRuleParser parser(output);

            lua_pushnil(L);
            while (lua_next(L, 1) != 0) {
                if (lua_type(L, -2) != LUA_TSTRING) {
                    lua_pop(L, 1);
                    continue;
                }

                const char* key = lua_tostring(L, -2);

                if (std::string_view{key} == "output") {
                    lua_pop(L, 1);
                    continue;
                }

                const SMonitorFieldDesc* desc = nullptr;
                for (const auto& fd : MONITOR_FIELDS) {
                    if (std::string_view{fd.name} == std::string_view{key}) {
                        desc = &fd;
                        break;
                    }
                }

                if (!desc) {
                    self->addError(std::format("{}: hl.monitor: unknown field '{}'", sourceInfo, key));
                    lua_pop(L, 1);
                    continue;
                }

                auto val = UP<ILuaConfigValue>(desc->factory());
                auto err = val->parse(L);
                if (err.errorCode != PARSE_ERROR_OK)
                    self->addError(std::format("{}: hl.monitor: field '{}': {}", sourceInfo, key, err.message));
                else if (!desc->apply(val.get(), parser))
                    self->addError(std::format("{}: hl.monitor: error applying field '{}'", sourceInfo, key));

                lua_pop(L, 1);
            }

            Config::monitorRuleMgr()->add(std::move(parser.rule()));
            return 0;
        },
        1);
    lua_setfield(m_lua, -2, "monitor");

    // hl.window_rule({name = "...", enabled = true, match = {...}, effect = value, ...})
    lua_pushlightuserdata(m_lua, this);
    lua_pushcclosure(
        m_lua,
        [](lua_State* L) -> int {
            auto* self = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

            if (!lua_istable(L, 1)) {
                self->addError("hl.window_rule: argument must be a table");
                return 0;
            }

            lua_Debug   ar         = {};
            std::string sourceInfo = "?:?";
            if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
                const char* src = ar.source;
                if (src && src[0] == '@')
                    src++;
                sourceInfo = std::format("{}:{}", src ? src : "?", ar.currentline);
            }

            // optional name - used for merge-on-redeclaration
            std::string name;
            lua_getfield(L, 1, "name");
            if (lua_isstring(L, -1))
                name = lua_tostring(L, -1);
            lua_pop(L, 1);

            // optional enabled flag (default true)
            bool enabled = true;
            lua_getfield(L, 1, "enabled");
            if (lua_isboolean(L, -1))
                enabled = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);

            // get or create the rule (merge if same name declared again)
            SP<Desktop::Rule::CWindowRule> rule;
            if (!name.empty() && self->m_luaWindowRules.contains(name)) {
                rule = self->m_luaWindowRules[name];
            } else {
                rule = makeShared<Desktop::Rule::CWindowRule>(name);
                if (!name.empty())
                    self->m_luaWindowRules[name] = rule;
                Desktop::Rule::ruleEngine()->registerRule(SP<Desktop::Rule::IRule>(rule));
            }
            rule->setEnabled(enabled);

            // parse match subtable - values may be string, bool, or number
            lua_getfield(L, 1, "match");
            if (lua_istable(L, -1)) {
                int matchIdx = lua_gettop(L);
                lua_pushnil(L);
                while (lua_next(L, matchIdx) != 0) {
                    if (lua_type(L, -2) == LUA_TSTRING) {
                        std::string matchKey = lua_tostring(L, -2);
                        std::string matchVal;
                        if (lua_type(L, -1) == LUA_TBOOLEAN)
                            matchVal = lua_toboolean(L, -1) ? "true" : "false";
                        else if (lua_type(L, -1) == LUA_TNUMBER)
                            matchVal = std::to_string(lua_tointeger(L, -1));
                        else if (lua_isstring(L, -1))
                            matchVal = lua_tostring(L, -1);
                        else {
                            self->addError(std::format("{}: hl.window_rule: match value for '{}' must be string, bool, or number", sourceInfo, matchKey));
                            lua_pop(L, 1);
                            continue;
                        }
                        auto prop = Desktop::Rule::matchPropFromString(matchKey);
                        if (prop.has_value())
                            rule->registerMatch(*prop, matchVal);
                        else
                            self->addError(std::format("{}: hl.window_rule: unknown match property '{}'", sourceInfo, matchKey));
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);

            // parse effect fields
            lua_pushnil(L);
            while (lua_next(L, 1) != 0) {
                if (lua_type(L, -2) != LUA_TSTRING) {
                    lua_pop(L, 1);
                    continue;
                }
                std::string_view key = lua_tostring(L, -2);
                if (key == "name" || key == "enabled" || key == "match") {
                    lua_pop(L, 1);
                    continue;
                }

                const SWindowRuleEffectDesc* desc = nullptr;
                for (const auto& d : WINDOW_RULE_EFFECT_DESCS) {
                    if (key == d.name) {
                        desc = &d;
                        break;
                    }
                }
                if (!desc) {
                    self->addError(std::format("{}: hl.window_rule: unknown field '{}'", sourceInfo, key));
                    lua_pop(L, 1);
                    continue;
                }

                auto val = UP<ILuaConfigValue>(desc->factory());
                auto err = val->parse(L);
                if (err.errorCode != PARSE_ERROR_OK)
                    self->addError(std::format("{}: hl.window_rule: field '{}': {}", sourceInfo, key, err.message));
                else
                    rule->addEffect(desc->effect, val->toString());

                lua_pop(L, 1);
            }

            return 0;
        },
        1);
    lua_setfield(m_lua, -2, "window_rule");

    // hl.layer_rule({name = "...", enabled = true, match = {namespace = "..."}, effect = value, ...})
    lua_pushlightuserdata(m_lua, this);
    lua_pushcclosure(
        m_lua,
        [](lua_State* L) -> int {
            auto* self = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

            if (!lua_istable(L, 1)) {
                self->addError("hl.layer_rule: argument must be a table");
                return 0;
            }

            lua_Debug   ar         = {};
            std::string sourceInfo = "?:?";
            if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
                const char* src = ar.source;
                if (src && src[0] == '@')
                    src++;
                sourceInfo = std::format("{}:{}", src ? src : "?", ar.currentline);
            }

            std::string name;
            lua_getfield(L, 1, "name");
            if (lua_isstring(L, -1))
                name = lua_tostring(L, -1);
            lua_pop(L, 1);

            bool enabled = true;
            lua_getfield(L, 1, "enabled");
            if (lua_isboolean(L, -1))
                enabled = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);

            SP<Desktop::Rule::CLayerRule> rule;
            if (!name.empty() && self->m_luaLayerRules.contains(name)) {
                rule = self->m_luaLayerRules[name];
            } else {
                rule = makeShared<Desktop::Rule::CLayerRule>(name);
                if (!name.empty())
                    self->m_luaLayerRules[name] = rule;
                Desktop::Rule::ruleEngine()->registerRule(SP<Desktop::Rule::IRule>(rule));
            }
            rule->setEnabled(enabled);

            lua_getfield(L, 1, "match");
            if (lua_istable(L, -1)) {
                int matchIdx = lua_gettop(L);
                lua_pushnil(L);
                while (lua_next(L, matchIdx) != 0) {
                    if (lua_type(L, -2) == LUA_TSTRING) {
                        std::string matchKey = lua_tostring(L, -2);
                        std::string matchVal;
                        if (lua_type(L, -1) == LUA_TBOOLEAN)
                            matchVal = lua_toboolean(L, -1) ? "true" : "false";
                        else if (lua_isstring(L, -1))
                            matchVal = lua_tostring(L, -1);
                        else {
                            self->addError(std::format("{}: hl.layer_rule: match value for '{}' must be string or bool", sourceInfo, matchKey));
                            lua_pop(L, 1);
                            continue;
                        }
                        auto prop = Desktop::Rule::matchPropFromString(matchKey);
                        if (prop.has_value())
                            rule->registerMatch(*prop, matchVal);
                        else
                            self->addError(std::format("{}: hl.layer_rule: unknown match property '{}'", sourceInfo, matchKey));
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);

            lua_pushnil(L);
            while (lua_next(L, 1) != 0) {
                if (lua_type(L, -2) != LUA_TSTRING) {
                    lua_pop(L, 1);
                    continue;
                }
                std::string_view key = lua_tostring(L, -2);
                if (key == "name" || key == "enabled" || key == "match") {
                    lua_pop(L, 1);
                    continue;
                }

                const SLayerRuleEffectDesc* desc = nullptr;
                for (const auto& d : LAYER_RULE_EFFECT_DESCS) {
                    if (key == d.name) {
                        desc = &d;
                        break;
                    }
                }
                if (!desc) {
                    self->addError(std::format("{}: hl.layer_rule: unknown field '{}'", sourceInfo, key));
                    lua_pop(L, 1);
                    continue;
                }

                auto val = UP<ILuaConfigValue>(desc->factory());
                auto err = val->parse(L);
                if (err.errorCode != PARSE_ERROR_OK)
                    self->addError(std::format("{}: hl.layer_rule: field '{}': {}", sourceInfo, key, err.message));
                else {
                    auto str = val->toString();
                    if (desc->invert)
                        str = (str == "1") ? "0" : "1";
                    rule->addEffect(desc->effect, str);
                }

                lua_pop(L, 1);
            }

            return 0;
        },
        1);
    lua_setfield(m_lua, -2, "layer_rule");

    // hl.workspace_rule({workspace = "...", enabled = true, gaps_out = 20, ...})
    lua_pushlightuserdata(m_lua, this);
    lua_pushcclosure(
        m_lua,
        [](lua_State* L) -> int {
            auto* self = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

            if (!lua_istable(L, 1)) {
                self->addError("hl.workspace_rule: argument must be a table");
                return 0;
            }

            lua_Debug   ar         = {};
            std::string sourceInfo = "?:?";
            if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
                const char* src = ar.source;
                if (src && src[0] == '@')
                    src++;
                sourceInfo = std::format("{}:{}", src ? src : "?", ar.currentline);
            }

            lua_getfield(L, 1, "workspace");
            if (!lua_isstring(L, -1)) {
                self->addError(std::format("{}: hl.workspace_rule: 'workspace' field is required and must be a string", sourceInfo));
                lua_pop(L, 1);
                return 0;
            }
            const std::string wsStr = lua_tostring(L, -1);
            lua_pop(L, 1);

            bool enabled = true;
            lua_getfield(L, 1, "enabled");
            if (lua_isboolean(L, -1))
                enabled = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);

            const auto& [wsId, wsName, isAutoID] = getWorkspaceIDNameFromString(wsStr);

            Config::CWorkspaceRule wsRule;
            wsRule.m_workspaceString = wsStr;
            wsRule.m_workspaceName   = wsName;
            wsRule.m_workspaceId     = isAutoID ? WORKSPACE_INVALID : wsId;
            wsRule.m_enabled         = enabled;

            lua_pushnil(L);
            while (lua_next(L, 1) != 0) {
                if (lua_type(L, -2) != LUA_TSTRING) {
                    lua_pop(L, 1);
                    continue;
                }
                std::string_view key = lua_tostring(L, -2);
                if (key == "workspace" || key == "enabled") {
                    lua_pop(L, 1);
                    continue;
                }

                const SWorkspaceRuleFieldDesc* desc = nullptr;
                for (const auto& d : WORKSPACE_RULE_FIELDS) {
                    if (key == d.name) {
                        desc = &d;
                        break;
                    }
                }
                if (!desc) {
                    self->addError(std::format("{}: hl.workspace_rule: unknown field '{}'", sourceInfo, key));
                    lua_pop(L, 1);
                    continue;
                }

                auto val = UP<ILuaConfigValue>(desc->factory());
                auto err = val->parse(L);
                if (err.errorCode != PARSE_ERROR_OK)
                    self->addError(std::format("{}: hl.workspace_rule: field '{}': {}", sourceInfo, key, err.message));
                else
                    desc->apply(val.get(), wsRule);

                lua_pop(L, 1);
            }

            Config::workspaceRuleMgr()->replaceOrAdd(std::move(wsRule));
            return 0;
        },
        1);
    lua_setfield(m_lua, -2, "workspace_rule");

    lua_setglobal(m_lua, "hl");
}

void CConfigManager::reinitLuaState() {
    // Destroy the event handler first so its luaL_unref calls happen while m_lua is still valid.
    m_eventHandler.reset();

    if (m_lua) {
        lua_close(m_lua);
        m_lua = nullptr;
    }

    m_lua = luaL_newstate();
    luaL_openlibs(m_lua);

    lua_pushlightuserdata(m_lua, this);
    lua_setfield(m_lua, LUA_REGISTRYINDEX, "hl_lua_manager");

    std::filesystem::path configDir = std::filesystem::path(m_mainConfigPath).parent_path();
    const std::string     luaPath   = (configDir / "?.lua").string() + ";" + (configDir / "?/init.lua").string();
    lua_getglobal(m_lua, "package");
    lua_pushstring(m_lua, luaPath.c_str());
    lua_setfield(m_lua, -2, "path");
    lua_pop(m_lua, 1);

    registerConfigTable();
    Bindings::registerBindings(m_lua, this);

    m_eventHandler = makeUnique<CLuaEventHandler>(m_lua);

    // Hook package.searchers[2] (the Lua file searcher) to track require()'d paths.
    lua_getglobal(m_lua, "package");
    lua_getfield(m_lua, -1, "searchers");
    lua_rawgeti(m_lua, -1, 2); // original file searcher
    lua_pushlightuserdata(m_lua, this);
    lua_pushcclosure(
        m_lua,
        [](lua_State* L) -> int {
            // upvalue 1: original searcher, upvalue 2: CConfigManager*
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_pushvalue(L, 1); // module name
            lua_call(L, 1, 2);   // -> loader?, filename?
            if (lua_isfunction(L, -2) && lua_isstring(L, -1)) {
                auto* self = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(2)));
                self->m_configPaths.emplace_back(lua_tostring(L, -1));
            }
            return 2;
        },
        2);
    lua_rawseti(m_lua, -2, 2); // replace package.searchers[2]
    lua_pop(m_lua, 2);         // pop searchers, package
}

void CConfigManager::init() {
    reinitLuaState();

    registerValue("general.border_size", new CLuaConfigInt(1, 0, 20));
    registerValue("general.gaps_in", new CLuaConfigCssGap(5));
    registerValue("general.gaps_out", new CLuaConfigCssGap(20));
    registerValue("general.float_gaps", new CLuaConfigCssGap(0));
    registerValue("general.gaps_workspaces", new CLuaConfigInt(0, 0, 100));
    registerValue("general.no_focus_fallback", new CLuaConfigBool(false));
    registerValue("general.resize_on_border", new CLuaConfigBool(false));
    registerValue("general.extend_border_grab_area", new CLuaConfigInt(15, 0, 100));
    registerValue("general.hover_icon_on_border", new CLuaConfigBool(true));
    registerValue("general.layout", new CLuaConfigString("dwindle"));
    registerValue("general.allow_tearing", new CLuaConfigBool(false));
    registerValue("general.resize_corner", new CLuaConfigInt(0, 0, 4));
    registerValue("general.snap.enabled", new CLuaConfigBool(false));
    registerValue("general.snap.window_gap", new CLuaConfigInt(10, 0, 100));
    registerValue("general.snap.monitor_gap", new CLuaConfigInt(10, 0, 100));
    registerValue("general.snap.border_overlap", new CLuaConfigBool(false));
    registerValue("general.snap.respect_gaps", new CLuaConfigBool(false));
    registerValue("general.col.active_border", new CLuaConfigGradient(CHyprColor(0xffffffff)));
    registerValue("general.col.inactive_border", new CLuaConfigGradient(CHyprColor(0xff444444)));
    registerValue("general.col.nogroup_border", new CLuaConfigGradient(CHyprColor(0xffffaaff)));
    registerValue("general.col.nogroup_border_active", new CLuaConfigGradient(CHyprColor(0xffff00ff)));
    registerValue("general.modal_parent_blocking", new CLuaConfigBool(true));
    registerValue("general.locale", new CLuaConfigString(""));

    registerValue("misc.disable_hyprland_logo", new CLuaConfigBool(false));
    registerValue("misc.disable_splash_rendering", new CLuaConfigBool(false));
    registerValue("misc.col.splash", new CLuaConfigInt(0x55ffffff));
    registerValue("misc.splash_font_family", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("misc.font_family", new CLuaConfigString("Sans"));
    registerValue("misc.force_default_wallpaper", new CLuaConfigInt(-1, -1, 2));
    registerValue("misc.vfr", new CLuaConfigBool(true));
    registerValue("misc.vrr", new CLuaConfigInt(0, 0, 3));
    registerValue("misc.mouse_move_enables_dpms", new CLuaConfigBool(false));
    registerValue("misc.key_press_enables_dpms", new CLuaConfigBool(false));
    registerValue("misc.name_vk_after_proc", new CLuaConfigBool(true));
    registerValue("misc.always_follow_on_dnd", new CLuaConfigBool(true));
    registerValue("misc.layers_hog_keyboard_focus", new CLuaConfigBool(true));
    registerValue("misc.animate_manual_resizes", new CLuaConfigBool(false));
    registerValue("misc.animate_mouse_windowdragging", new CLuaConfigBool(false));
    registerValue("misc.disable_autoreload", new CLuaConfigBool(false));
    registerValue("misc.enable_swallow", new CLuaConfigBool(false));
    registerValue("misc.swallow_regex", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("misc.swallow_exception_regex", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("misc.focus_on_activate", new CLuaConfigBool(false));
    registerValue("misc.mouse_move_focuses_monitor", new CLuaConfigBool(true));
    registerValue("misc.allow_session_lock_restore", new CLuaConfigBool(false));
    registerValue("misc.session_lock_xray", new CLuaConfigBool(false));
    registerValue("misc.close_special_on_empty", new CLuaConfigBool(true));
    registerValue("misc.background_color", new CLuaConfigInt(0xff111111));
    registerValue("misc.on_focus_under_fullscreen",
                  new CLuaConfigInt(2, 0, 2, std::unordered_map<std::string, Config::INTEGER>{{"ignore", 0}, {"take_over", 1}, {"exit_fullscreen", 2}}));
    registerValue("misc.exit_window_retains_fullscreen", new CLuaConfigBool(false));
    registerValue("misc.initial_workspace_tracking", new CLuaConfigInt(1, 0, 2));
    registerValue("misc.middle_click_paste", new CLuaConfigBool(true));
    registerValue("misc.render_unfocused_fps", new CLuaConfigInt(15, 1, 120));
    registerValue("misc.disable_xdg_env_checks", new CLuaConfigBool(false));
    registerValue("misc.disable_hyprland_guiutils_check", new CLuaConfigBool(false));
    registerValue("misc.disable_watchdog_warning", new CLuaConfigBool(false));
    registerValue("misc.lockdead_screen_delay", new CLuaConfigInt(1000, 0, 5000));
    registerValue("misc.enable_anr_dialog", new CLuaConfigBool(true));
    registerValue("misc.anr_missed_pings", new CLuaConfigInt(5, 1, 20));
    registerValue("misc.screencopy_force_8b", new CLuaConfigBool(true));
    registerValue("misc.disable_scale_notification", new CLuaConfigBool(false));
    registerValue("misc.size_limits_tiled", new CLuaConfigBool(false));

    registerValue("group.insert_after_current", new CLuaConfigBool(true));
    registerValue("group.focus_removed_window", new CLuaConfigBool(true));
    registerValue("group.merge_groups_on_drag", new CLuaConfigBool(true));
    registerValue("group.merge_groups_on_groupbar", new CLuaConfigBool(true));
    registerValue("group.merge_floated_into_tiled_on_groupbar", new CLuaConfigBool(false));
    registerValue("group.auto_group", new CLuaConfigBool(true));
    registerValue("group.drag_into_group", new CLuaConfigInt(1, 0, 2));
    registerValue("group.group_on_movetoworkspace", new CLuaConfigBool(false));
    registerValue("group.groupbar.enabled", new CLuaConfigBool(true));
    registerValue("group.groupbar.font_family", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("group.groupbar.font_weight_active", new CLuaConfigFontWeight());
    registerValue("group.groupbar.font_weight_inactive", new CLuaConfigFontWeight());
    registerValue("group.groupbar.font_size", new CLuaConfigInt(8, 2, 64));
    registerValue("group.groupbar.gradients", new CLuaConfigBool(false));
    registerValue("group.groupbar.height", new CLuaConfigInt(14, 1, 64));
    registerValue("group.groupbar.indicator_gap", new CLuaConfigInt(0, 0, 64));
    registerValue("group.groupbar.indicator_height", new CLuaConfigInt(3, 1, 64));
    registerValue("group.groupbar.priority", new CLuaConfigInt(3, 0, 6));
    registerValue("group.groupbar.render_titles", new CLuaConfigBool(true));
    registerValue("group.groupbar.scrolling", new CLuaConfigBool(true));
    registerValue("group.groupbar.text_color", new CLuaConfigColor(0xffffffff));
    registerValue("group.groupbar.text_color_inactive", new CLuaConfigColor(-1));
    registerValue("group.groupbar.text_color_locked_active", new CLuaConfigColor(-1));
    registerValue("group.groupbar.text_color_locked_inactive", new CLuaConfigColor(-1));
    registerValue("group.groupbar.stacked", new CLuaConfigBool(false));
    registerValue("group.groupbar.rounding", new CLuaConfigInt(1, 0, 20));
    registerValue("group.groupbar.rounding_power", new CLuaConfigFloat(2.F, 2.F, 10.F));
    registerValue("group.groupbar.gradient_rounding", new CLuaConfigInt(2, 0, 20));
    registerValue("group.groupbar.gradient_rounding_power", new CLuaConfigFloat(2.F, 2.F, 10.F));
    registerValue("group.groupbar.round_only_edges", new CLuaConfigBool(true));
    registerValue("group.groupbar.gradient_round_only_edges", new CLuaConfigBool(true));
    registerValue("group.groupbar.gaps_out", new CLuaConfigInt(2, 0, 20));
    registerValue("group.groupbar.gaps_in", new CLuaConfigInt(2, 0, 20));
    registerValue("group.groupbar.keep_upper_gap", new CLuaConfigBool(true));
    registerValue("group.groupbar.text_offset", new CLuaConfigInt(0, -20, 20));
    registerValue("group.groupbar.text_padding", new CLuaConfigInt(0, 0, 22));
    registerValue("group.groupbar.blur", new CLuaConfigBool(false));

    registerValue("debug.log_damage", new CLuaConfigBool(false));
    registerValue("debug.overlay", new CLuaConfigBool(false));
    registerValue("debug.damage_blink", new CLuaConfigBool(false));
    registerValue("debug.pass", new CLuaConfigBool(false));
    registerValue("debug.gl_debugging", new CLuaConfigBool(false));
    registerValue("debug.disable_logs", new CLuaConfigBool(true));
    registerValue("debug.disable_time", new CLuaConfigBool(true));
    registerValue("debug.enable_stdout_logs", new CLuaConfigBool(false));
    registerValue("debug.damage_tracking", new CLuaConfigInt(sc<Config::INTEGER>(DAMAGE_TRACKING_FULL), 0, 2));
    registerValue("debug.manual_crash", new CLuaConfigInt(0, 0, 1));
    registerValue("debug.suppress_errors", new CLuaConfigBool(false));
    registerValue("debug.error_limit", new CLuaConfigInt(5, 0, 20));
    registerValue("debug.error_position", new CLuaConfigInt(0, 0, 1));
    registerValue("debug.disable_scale_checks", new CLuaConfigBool(false));
    registerValue("debug.colored_stdout_logs", new CLuaConfigBool(true));
    registerValue("debug.full_cm_proto", new CLuaConfigBool(false));
    registerValue("debug.ds_handle_same_buffer", new CLuaConfigBool(true));
    registerValue("debug.ds_handle_same_buffer_fifo", new CLuaConfigBool(true));
    registerValue("debug.fifo_pending_workaround", new CLuaConfigBool(false));
    registerValue("debug.render_solitary_wo_damage", new CLuaConfigBool(false));

    registerValue("decoration.rounding", new CLuaConfigInt(0, 0, 20));
    registerValue("decoration.rounding_power", new CLuaConfigFloat(2.F, 2.F, 10.F));
    registerValue("decoration.blur.enabled", new CLuaConfigBool(true));
    registerValue("decoration.blur.size", new CLuaConfigInt(8, 0, 100));
    registerValue("decoration.blur.passes", new CLuaConfigInt(1, 0, 10));
    registerValue("decoration.blur.ignore_opacity", new CLuaConfigBool(true));
    registerValue("decoration.blur.new_optimizations", new CLuaConfigBool(true));
    registerValue("decoration.blur.xray", new CLuaConfigBool(false));
    registerValue("decoration.blur.contrast", new CLuaConfigFloat(0.8916F, 0.F, 2.F));
    registerValue("decoration.blur.brightness", new CLuaConfigFloat(1.0F, 0.F, 2.F));
    registerValue("decoration.blur.vibrancy", new CLuaConfigFloat(0.1696F, 0.F, 1.F));
    registerValue("decoration.blur.vibrancy_darkness", new CLuaConfigFloat(0.0F, 0.F, 1.F));
    registerValue("decoration.blur.noise", new CLuaConfigFloat(0.0117F, 0.F, 1.F));
    registerValue("decoration.blur.special", new CLuaConfigBool(false));
    registerValue("decoration.blur.popups", new CLuaConfigBool(false));
    registerValue("decoration.blur.popups_ignorealpha", new CLuaConfigFloat(0.2F, 0.F, 1.F));
    registerValue("decoration.blur.input_methods", new CLuaConfigBool(false));
    registerValue("decoration.blur.input_methods_ignorealpha", new CLuaConfigFloat(0.2F, 0.F, 1.F));
    registerValue("decoration.active_opacity", new CLuaConfigFloat(1.F, 0.F, 1.F));
    registerValue("decoration.inactive_opacity", new CLuaConfigFloat(1.F, 0.F, 1.F));
    registerValue("decoration.fullscreen_opacity", new CLuaConfigFloat(1.F, 0.F, 1.F));
    registerValue("decoration.shadow.enabled", new CLuaConfigBool(true));
    registerValue("decoration.shadow.range", new CLuaConfigInt(4, 0, 100));
    registerValue("decoration.shadow.render_power", new CLuaConfigInt(3, 1, 4));
    registerValue("decoration.shadow.ignore_window", new CLuaConfigBool(true));
    registerValue("decoration.shadow.offset", new CLuaConfigVec2({0, 0}));
    registerValue("decoration.shadow.scale", new CLuaConfigFloat(1.F, 0.F, 1.F));
    registerValue("decoration.shadow.sharp", new CLuaConfigBool(false));
    registerValue("decoration.shadow.color", new CLuaConfigColor(0xee1a1a1a));
    registerValue("decoration.shadow.color_inactive", new CLuaConfigColor(-1));
    registerValue("decoration.dim_inactive", new CLuaConfigBool(false));
    registerValue("decoration.dim_modal", new CLuaConfigBool(true));
    registerValue("decoration.dim_strength", new CLuaConfigFloat(0.5F, 0.F, 1.F));
    registerValue("decoration.dim_special", new CLuaConfigFloat(0.2F, 0.F, 1.F));
    registerValue("decoration.dim_around", new CLuaConfigFloat(0.4F, 0.F, 1.F));
    registerValue("decoration.screen_shader", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("decoration.border_part_of_window", new CLuaConfigBool(true));

    registerValue("layout.single_window_aspect_ratio", new CLuaConfigVec2({0, 0}));
    registerValue("layout.single_window_aspect_ratio_tolerance", new CLuaConfigFloat(0.1F, 0.F, 1.F));

    registerValue("dwindle.pseudotile", new CLuaConfigBool(false));
    registerValue("dwindle.force_split", new CLuaConfigInt(0, 0, 2));
    registerValue("dwindle.permanent_direction_override", new CLuaConfigBool(false));
    registerValue("dwindle.preserve_split", new CLuaConfigBool(false));
    registerValue("dwindle.special_scale_factor", new CLuaConfigFloat(1.F, 0.F, 1.F));
    registerValue("dwindle.split_width_multiplier", new CLuaConfigFloat(1.0F, 0.1F, 3.F));
    registerValue("dwindle.use_active_for_splits", new CLuaConfigBool(true));
    registerValue("dwindle.default_split_ratio", new CLuaConfigFloat(1.F, 0.1F, 1.9F));
    registerValue("dwindle.split_bias", new CLuaConfigInt(0, 0, 1));
    registerValue("dwindle.smart_split", new CLuaConfigBool(false));
    registerValue("dwindle.smart_resizing", new CLuaConfigBool(true));
    registerValue("dwindle.precise_mouse_move", new CLuaConfigBool(false));

    registerValue("master.special_scale_factor", new CLuaConfigFloat(1.F, 0.F, 1.F));
    registerValue("master.mfact", new CLuaConfigFloat(0.55F, 0.F, 1.F));
    registerValue("master.new_status", new CLuaConfigString("slave"));
    registerValue("master.slave_count_for_center_master", new CLuaConfigInt(2, 0, 10));
    registerValue("master.center_master_fallback", new CLuaConfigString("left"));
    registerValue("master.center_ignores_reserved", new CLuaConfigBool(false));
    registerValue("master.new_on_active", new CLuaConfigString("none"));
    registerValue("master.new_on_top", new CLuaConfigBool(false));
    registerValue("master.orientation", new CLuaConfigString("left"));
    registerValue("master.allow_small_split", new CLuaConfigBool(false));
    registerValue("master.smart_resizing", new CLuaConfigBool(true));
    registerValue("master.drop_at_cursor", new CLuaConfigBool(true));
    registerValue("master.always_keep_position", new CLuaConfigBool(false));

    registerValue("scrolling.fullscreen_on_one_column", new CLuaConfigBool(true));
    registerValue("scrolling.column_width", new CLuaConfigFloat(0.5F, 0.1F, 1.0F));
    registerValue("scrolling.focus_fit_method", new CLuaConfigInt(1, 0, 1));
    registerValue("scrolling.follow_focus", new CLuaConfigBool(true));
    registerValue("scrolling.follow_min_visible", new CLuaConfigFloat(0.4F, 0.0F, 1.0F));
    registerValue("scrolling.explicit_column_widths", new CLuaConfigString("0.333, 0.5, 0.667, 1.0"));
    registerValue("scrolling.direction", new CLuaConfigString("right"));
    registerValue("scrolling.wrap_focus", new CLuaConfigBool(true));
    registerValue("scrolling.wrap_swapcol", new CLuaConfigBool(true));

    registerValue("animations.enabled", new CLuaConfigBool(true));
    registerValue("animations.workspace_wraparound", new CLuaConfigBool(false));

    registerValue("input.follow_mouse", new CLuaConfigInt(1, 0, 3));
    registerValue("input.follow_mouse_threshold", new CLuaConfigFloat(0.F));
    registerValue("input.focus_on_close", new CLuaConfigInt(0, 0, 1));
    registerValue("input.mouse_refocus", new CLuaConfigBool(true));
    registerValue("input.special_fallthrough", new CLuaConfigBool(false));
    registerValue("input.off_window_axis_events", new CLuaConfigInt(1, 0, 3));
    registerValue("input.sensitivity", new CLuaConfigFloat(0.F, -1.F, 1.F));
    registerValue("input.accel_profile", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.rotation", new CLuaConfigInt(0, 0, 359));
    registerValue("input.kb_file", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.kb_layout", new CLuaConfigString("us"));
    registerValue("input.kb_variant", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.kb_options", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.kb_rules", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.kb_model", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.repeat_rate", new CLuaConfigInt(25, 0, 200));
    registerValue("input.repeat_delay", new CLuaConfigInt(600, 0, 2000));
    registerValue("input.natural_scroll", new CLuaConfigBool(false));
    registerValue("input.numlock_by_default", new CLuaConfigBool(false));
    registerValue("input.resolve_binds_by_sym", new CLuaConfigBool(false));
    registerValue("input.force_no_accel", new CLuaConfigBool(false));
    registerValue("input.float_switch_override_focus", new CLuaConfigInt(1, 0, 2));
    registerValue("input.left_handed", new CLuaConfigBool(false));
    registerValue("input.scroll_method", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.scroll_button", new CLuaConfigInt(0, 0, 300));
    registerValue("input.scroll_button_lock", new CLuaConfigBool(false));
    registerValue("input.scroll_factor", new CLuaConfigFloat(1.F, 0.F, 2.F));
    registerValue("input.scroll_points", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.emulate_discrete_scroll", new CLuaConfigInt(1, 0, 2));
    registerValue("input.touchpad.natural_scroll", new CLuaConfigBool(false));
    registerValue("input.touchpad.disable_while_typing", new CLuaConfigBool(true));
    registerValue("input.touchpad.clickfinger_behavior", new CLuaConfigBool(false));
    registerValue("input.touchpad.tap_button_map", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.touchpad.middle_button_emulation", new CLuaConfigBool(false));
    registerValue("input.touchpad.tap-to-click", new CLuaConfigBool(true));
    registerValue("input.touchpad.tap-and-drag", new CLuaConfigBool(true));
    registerValue("input.touchpad.drag_lock", new CLuaConfigInt(0, 0, 2));
    registerValue("input.touchpad.scroll_factor", new CLuaConfigFloat(1.F, 0.F, 2.F));
    registerValue("input.touchpad.flip_x", new CLuaConfigBool(false));
    registerValue("input.touchpad.flip_y", new CLuaConfigBool(false));
    registerValue("input.touchpad.drag_3fg", new CLuaConfigInt(0, 0, 2));
    registerValue("input.touchdevice.transform", new CLuaConfigInt(-1));
    registerValue("input.touchdevice.output", new CLuaConfigString("[[Auto]]"));
    registerValue("input.touchdevice.enabled", new CLuaConfigBool(true));
    registerValue("input.virtualkeyboard.share_states", new CLuaConfigInt(2, 0, 2));
    registerValue("input.virtualkeyboard.release_pressed_on_close", new CLuaConfigBool(false));
    registerValue("input.tablet.transform", new CLuaConfigInt(0, 0, 6));
    registerValue("input.tablet.output", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("input.tablet.region_position", new CLuaConfigVec2({0, 0}));
    registerValue("input.tablet.absolute_region_position", new CLuaConfigBool(false));
    registerValue("input.tablet.region_size", new CLuaConfigVec2({0, 0}));
    registerValue("input.tablet.relative_input", new CLuaConfigBool(false));
    registerValue("input.tablet.left_handed", new CLuaConfigBool(false));
    registerValue("input.tablet.active_area_position", new CLuaConfigVec2({0, 0}));
    registerValue("input.tablet.active_area_size", new CLuaConfigVec2({0, 0}));

    registerValue("binds.pass_mouse_when_bound", new CLuaConfigBool(false));
    registerValue("binds.scroll_event_delay", new CLuaConfigInt(300, 0, 2000));
    registerValue("binds.workspace_back_and_forth", new CLuaConfigBool(false));
    registerValue("binds.hide_special_on_workspace_change", new CLuaConfigBool(false));
    registerValue("binds.allow_workspace_cycles", new CLuaConfigBool(false));
    registerValue("binds.workspace_center_on", new CLuaConfigInt(1, 0, 1));
    registerValue("binds.focus_preferred_method", new CLuaConfigInt(0, 0, 1));
    registerValue("binds.ignore_group_lock", new CLuaConfigBool(false));
    registerValue("binds.movefocus_cycles_fullscreen", new CLuaConfigBool(false));
    registerValue("binds.movefocus_cycles_groupfirst", new CLuaConfigBool(false));
    registerValue("binds.disable_keybind_grabbing", new CLuaConfigBool(false));
    registerValue("binds.allow_pin_fullscreen", new CLuaConfigBool(false));
    registerValue("binds.drag_threshold", new CLuaConfigInt(0, 0, INT_MAX));
    registerValue("binds.window_direction_monitor_fallback", new CLuaConfigBool(true));

    registerValue("gestures.workspace_swipe_distance", new CLuaConfigInt(300, 0, 2000));
    registerValue("gestures.workspace_swipe_invert", new CLuaConfigBool(true));
    registerValue("gestures.workspace_swipe_min_speed_to_force", new CLuaConfigInt(30, 0, 200));
    registerValue("gestures.workspace_swipe_cancel_ratio", new CLuaConfigFloat(0.5F, 0.F, 1.F));
    registerValue("gestures.workspace_swipe_create_new", new CLuaConfigBool(true));
    registerValue("gestures.workspace_swipe_direction_lock", new CLuaConfigBool(true));
    registerValue("gestures.workspace_swipe_direction_lock_threshold", new CLuaConfigInt(10, 0, 200));
    registerValue("gestures.workspace_swipe_forever", new CLuaConfigBool(false));
    registerValue("gestures.workspace_swipe_use_r", new CLuaConfigBool(false));
    registerValue("gestures.workspace_swipe_touch", new CLuaConfigBool(false));
    registerValue("gestures.workspace_swipe_touch_invert", new CLuaConfigBool(false));
    registerValue("gestures.close_max_timeout", new CLuaConfigInt(1000, 10, 2000));

    registerValue("xwayland.enabled", new CLuaConfigBool(true));
    registerValue("xwayland.use_nearest_neighbor", new CLuaConfigBool(true));
    registerValue("xwayland.force_zero_scaling", new CLuaConfigBool(false));
    registerValue("xwayland.create_abstract_socket", new CLuaConfigBool(false));

    registerValue("opengl.nvidia_anti_flicker", new CLuaConfigBool(true));

    registerValue("cursor.invisible", new CLuaConfigBool(false));
    registerValue("cursor.no_hardware_cursors", new CLuaConfigInt(2, 0, 2));
    registerValue("cursor.no_break_fs_vrr", new CLuaConfigInt(2, 0, 2));
    registerValue("cursor.min_refresh_rate", new CLuaConfigInt(24, 10, 500));
    registerValue("cursor.hotspot_padding", new CLuaConfigInt(0, 0, 20));
    registerValue("cursor.inactive_timeout", new CLuaConfigFloat(0.F));
    registerValue("cursor.no_warps", new CLuaConfigBool(false));
    registerValue("cursor.persistent_warps", new CLuaConfigBool(false));
    registerValue("cursor.warp_on_change_workspace", new CLuaConfigInt(0, 0, 2));
    registerValue("cursor.warp_on_toggle_special", new CLuaConfigInt(0, 0, 2));
    registerValue("cursor.default_monitor", new CLuaConfigString(STRVAL_EMPTY));
    registerValue("cursor.zoom_factor", new CLuaConfigFloat(1.F, 1.F, 10.F));
    registerValue("cursor.zoom_rigid", new CLuaConfigBool(false));
    registerValue("cursor.zoom_disable_aa", new CLuaConfigBool(false));
    registerValue("cursor.zoom_detached_camera", new CLuaConfigBool(true));
    registerValue("cursor.enable_hyprcursor", new CLuaConfigBool(true));
    registerValue("cursor.sync_gsettings_theme", new CLuaConfigBool(true));
    registerValue("cursor.hide_on_key_press", new CLuaConfigBool(false));
    registerValue("cursor.hide_on_touch", new CLuaConfigBool(true));
    registerValue("cursor.hide_on_tablet", new CLuaConfigBool(false));
    registerValue("cursor.use_cpu_buffer", new CLuaConfigInt(2, 0, 2));
    registerValue("cursor.warp_back_after_non_mouse_input", new CLuaConfigBool(false));

    registerValue("autogenerated", new CLuaConfigInt(0));

    registerValue("group.col.border_active", new CLuaConfigGradient(CHyprColor(0x66ffff00)));
    registerValue("group.col.border_inactive", new CLuaConfigGradient(CHyprColor(0x66777700)));
    registerValue("group.col.border_locked_active", new CLuaConfigGradient(CHyprColor(0x66ff5500)));
    registerValue("group.col.border_locked_inactive", new CLuaConfigGradient(CHyprColor(0x66775500)));

    registerValue("group.groupbar.col.active", new CLuaConfigGradient(CHyprColor(0x66ffff00)));
    registerValue("group.groupbar.col.inactive", new CLuaConfigGradient(CHyprColor(0x66777700)));
    registerValue("group.groupbar.col.locked_active", new CLuaConfigGradient(CHyprColor(0x66ff5500)));
    registerValue("group.groupbar.col.locked_inactive", new CLuaConfigGradient(CHyprColor(0x66775500)));

    registerValue("render.direct_scanout", new CLuaConfigInt(0, 0, 2));
    registerValue("render.expand_undersized_textures", new CLuaConfigBool(true));
    registerValue("render.xp_mode", new CLuaConfigBool(false));
    registerValue("render.ctm_animation", new CLuaConfigInt(2, 0, 2));
    registerValue("render.cm_fs_passthrough", new CLuaConfigInt(2, 0, 2));
    registerValue("render.cm_enabled", new CLuaConfigBool(true));
    registerValue("render.send_content_type", new CLuaConfigBool(true));
    registerValue("render.cm_auto_hdr", new CLuaConfigInt(1, 0, 2));
    registerValue("render.new_render_scheduling", new CLuaConfigBool(false));
    registerValue("render.non_shader_cm", new CLuaConfigInt(3, 0, 3));
    registerValue("render.cm_sdr_eotf", new CLuaConfigString("default"));
    registerValue("render.commit_timing_enabled", new CLuaConfigBool(true));
    registerValue("render.icc_vcgt_enabled", new CLuaConfigBool(true));
    registerValue("render.use_shader_blur_blend", new CLuaConfigBool(false));

    registerValue("ecosystem.no_update_news", new CLuaConfigBool(false));
    registerValue("ecosystem.no_donation_nag", new CLuaConfigBool(false));
    registerValue("ecosystem.enforce_permissions", new CLuaConfigBool(false));

    registerValue("quirks.prefer_hdr", new CLuaConfigInt(0, 0, 2));
    registerValue("quirks.skip_non_kms_dmabuf_formats", new CLuaConfigBool(false));

    Config::watcher()->setOnChange([this](const CConfigWatcher::SConfigWatchEvent& e) {
        Log::logger->log(Log::DEBUG, "[lua] file {} modified, reloading", e.file);
        reload();
    });

    reload();
}

void CConfigManager::reload() {
    Event::bus()->m_events.config.preReload.emit();

    m_mainConfigPath = Supplementary::Jeremy::getMainConfigPath()->path;

    // reset tracked paths; the searcher hook will re-populate them as require() runs
    m_configPaths.clear();
    m_configPaths.emplace_back(m_mainConfigPath);

    // clear package.loaded for user modules so require() re-executes them and
    // the searcher hook can re-track their paths.
    lua_getglobal(m_lua, "package");
    lua_getfield(m_lua, -1, "loaded");
    static constexpr std::string_view STDLIB[] = {"_G", "coroutine", "debug", "io", "math", "os", "package", "string", "table", "utf8", "bit32", "jit"};
    lua_pushnil(m_lua);
    while (lua_next(m_lua, -2)) {
        lua_pop(m_lua, 1); // pop value, keep key
        if (lua_isstring(m_lua, -1)) {
            std::string_view mod  = lua_tostring(m_lua, -1);
            bool             skip = false;
            for (const auto& s : STDLIB) {
                if (mod == s) {
                    skip = true;
                    break;
                }
            }
            if (!skip) {
                lua_pushvalue(m_lua, -1); // dup key
                lua_pushnil(m_lua);
                lua_settable(m_lua, -4); // package.loaded[key] = nil
            }
        }
    }
    lua_pop(m_lua, 2); // pop loaded, package

    // phase 1: check syntax before clearing any state, so a broken syntax
    // doesn't entirely fucking nuke the config and leave the user
    // with no binds.
    //
    // this won't help if they are launching hyprland,
    // which is a FIXME: add some recovery binds...
    if (luaL_loadfile(m_lua, m_mainConfigPath.c_str()) != LUA_OK) {
        m_errors.clear();
        addError(lua_tostring(m_lua, -1));
        lua_pop(m_lua, 1);
        m_lastConfigVerificationWasSuccessful = false;
        postConfigReload();
        return;
    }

    // phase 2: syntax is valid, reset and load.
    Config::animationTree()->reset();
    Config::workspaceRuleMgr()->clear();
    Config::monitorRuleMgr()->clear();
    Desktop::Rule::ruleEngine()->clearAllRules();
    m_luaWindowRules.clear();
    m_luaLayerRules.clear();
    m_errors.clear();
    m_deviceConfigs.clear();

    if (g_pKeybindManager)
        g_pKeybindManager->clearKeybinds();

    lua_pushcfunction(m_lua, [](lua_State* L) -> int {
        luaL_traceback(L, L, lua_tostring(L, 1), 1);
        return 1;
    });
    lua_insert(m_lua, 1);

    if (lua_pcall(m_lua, 0, 0, 1) != LUA_OK) {
        addError(lua_tostring(m_lua, -1));
        lua_pop(m_lua, 1);
        m_lastConfigVerificationWasSuccessful = false;
    } else
        m_lastConfigVerificationWasSuccessful = m_errors.empty();

    lua_remove(m_lua, 1);

    postConfigReload();

    m_isFirstLaunch = false;
}

void CConfigManager::postConfigReload() {

    static auto PZOOMFACTOR     = CConfigValue<Config::FLOAT>("cursor.zoom_factor");
    static auto PSUPPRESSERRORS = CConfigValue<Config::INTEGER>("debug.suppress_errors");
    static auto PXWAYLAND       = CConfigValue<Config::INTEGER>("xwayland.enabled");
    static auto PMANUALCRASH    = CConfigValue<Config::INTEGER>("debug.manual_crash");
    static auto PENABLESTDOUT   = CConfigValue<Config::INTEGER>("debug.enable_stdout_logs");
    static auto PAUTOGENERATED  = CConfigValue<Config::INTEGER>("autogenerated");

    Config::watcher()->update();

    for (auto const& w : g_pCompositor->m_windows) {
        w->uncacheWindowDecos();
    }

    for (auto const& m : g_pCompositor->m_monitors) {
        *(m->m_cursorZoom) = *PZOOMFACTOR;
        if (m->m_activeWorkspace)
            m->m_activeWorkspace->m_space->recalculate();
    }

    // Update the keyboard layout to the cfg'd one if this is not the first launch
    if (!m_isFirstLaunch) {
        g_pInputManager->setKeyboardLayout();
        g_pInputManager->setPointerConfigs();
        g_pInputManager->setTouchDeviceConfigs();
        g_pInputManager->setTabletConfigs();

        g_pHyprRenderer->m_reloadScreenShader = true;
    }

    // parseError will be displayed next frame
    if (!m_errors.empty() && !*PSUPPRESSERRORS) {
        std::string errorStr = "Your config has errors:\n";
        for (const auto& e : m_errors) {
            errorStr += e + "\n";

            if (std::ranges::count(errorStr, '\n') > 10) {
                errorStr += "... more";
                break;
            }
        }

        if (!errorStr.empty() && errorStr.back() == '\n')
            errorStr.pop_back();

        g_pHyprError->queueCreate(errorStr, CHyprColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
    } else if (*PAUTOGENERATED)
        g_pHyprError->queueCreate(
            "Warning: You're using an autogenerated config! Edit the config file to get rid of this message. (config file: " + getMainConfigPath() +
                " )\nSUPER+Q -> kitty (if it doesn't launch, make sure it's installed or choose a different terminal in the config)\nSUPER+M -> exit Hyprland",
            CHyprColor(1.0, 1.0, 70.0 / 255.0, 1.0));
    else
        g_pHyprError->destroy();

    // Set the modes for all monitors as we configured them
    // not on first launch because monitors might not exist yet
    // and they'll be taken care of in the newMonitor event
    if (!m_isFirstLaunch) {
        // check
        Config::monitorRuleMgr()->scheduleReload();
        Config::monitorRuleMgr()->ensureMonitorStatus();
        Config::monitorRuleMgr()->ensureVRR();
    }

#ifndef NO_XWAYLAND
    g_pCompositor->m_wantsXwayland = *PXWAYLAND;
    // enable/disable xwayland usage
    if (!m_isFirstLaunch &&
        g_pXWayland /* XWayland has to be initialized by CCompositor::initManagers for this to make sense, and it doesn't have to be (e.g. very early plugin load) */) {
        bool prevEnabledXwayland = g_pXWayland->enabled();
        if (g_pCompositor->m_wantsXwayland != prevEnabledXwayland)
            g_pXWayland = makeUnique<CXWayland>(g_pCompositor->m_wantsXwayland);
    } else
        g_pCompositor->m_wantsXwayland = *PXWAYLAND;
#endif

    if (!m_isFirstLaunch && !g_pCompositor->m_unsafeState)
        refreshGroupBarGradients();

    for (auto const& w : g_pCompositor->getWorkspaces()) {
        if (w->inert())
            continue;
        w->updateWindows();
        w->updateWindowData();
    }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (*PMANUALCRASH && !m_manualCrashInitiated) {
        m_manualCrashInitiated = true;
        g_pHyprNotificationOverlay->addNotification("Manual crash has been set up. Set debug.manual_crash back to 0 in order to crash the compositor.", CHyprColor(0), 5000,
                                                    ICON_INFO);
    } else if (m_manualCrashInitiated && !*PMANUALCRASH) {
        // cowabunga it is
        g_pHyprRenderer->initiateManualCrash();
    }

    auto disableStdout = !*PENABLESTDOUT;
    if (disableStdout && m_isFirstLaunch)
        Log::logger->log(Log::DEBUG, "Disabling stdout logs! Check the log for further logs.");

    for (auto const& m : g_pCompositor->m_monitors) {
        // mark blur dirty
        m->m_blurFBDirty = true;

        g_pCompositor->scheduleFrameForMonitor(m);

        // Force the compositor to fully re-render all monitors
        m->m_forceFullFrames = 2;

        // also force mirrors, as the aspect ratio could've changed
        for (auto const& mirror : m->m_mirrors)
            mirror->m_forceFullFrames = 3;
    }

    handlePluginLoads();

    if (!m_isFirstLaunch)
        g_pCompositor->ensurePersistentWorkspacesPresent();

    Layout::Supplementary::algoMatcher()->updateWorkspaceLayouts();

    Event::bus()->m_events.config.reloaded.emit();
    if (g_pEventManager)
        g_pEventManager->postEvent(SHyprIPCEvent{"configreloaded", ""});
}

void CConfigManager::addError(std::string&& str) {
    m_errors.emplace_back(std::move(str));
}

std::string CConfigManager::verify() {
    // todo
    return "FIXME: Not impl'd";
}

static std::string normalizeDeviceName(const std::string& dev) {
    auto copy = dev;
    std::ranges::replace(copy, ' ', '-');
    return copy;
}

ILuaConfigValue* CConfigManager::findDeviceValue(const std::string& dev, const std::string& field) {
    const auto devIt = m_deviceConfigs.find(dev);
    if (devIt == m_deviceConfigs.end())
        return nullptr;
    const auto valIt = devIt->second.values.find(field);
    return valIt != devIt->second.values.end() ? valIt->second.get() : nullptr;
}

int CConfigManager::getDeviceInt(const std::string& dev, const std::string& field, const std::string& fallback) {
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), field))
        return (int)*static_cast<const Config::INTEGER*>(v->data());
    if (!fallback.empty() && m_configValues.contains(fallback))
        return (int)*static_cast<const Config::INTEGER*>(m_configValues.at(fallback)->data());
    return 0;
}

float CConfigManager::getDeviceFloat(const std::string& dev, const std::string& field, const std::string& fallback) {
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), field))
        return *static_cast<const Config::FLOAT*>(v->data());
    if (!fallback.empty() && m_configValues.contains(fallback))
        return *static_cast<const Config::FLOAT*>(m_configValues.at(fallback)->data());
    return 0.F;
}

Vector2D CConfigManager::getDeviceVec(const std::string& dev, const std::string& field, const std::string& fallback) {
    auto toVec = [](const Config::VEC2& v) -> Vector2D { return {v.x, v.y}; };
    if (auto* val = findDeviceValue(normalizeDeviceName(dev), field))
        return toVec(*static_cast<const Config::VEC2*>(val->data()));
    if (!fallback.empty() && m_configValues.contains(fallback))
        return toVec(*static_cast<const Config::VEC2*>(m_configValues.at(fallback)->data()));
    return {0, 0};
}

std::string CConfigManager::getDeviceString(const std::string& dev, const std::string& field, const std::string& fallback) {
    auto clean = [](const Config::STRING& s) -> std::string { return s == STRVAL_EMPTY ? "" : s; };
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), field))
        return clean(*static_cast<const Config::STRING*>(v->data()));
    if (!fallback.empty() && m_configValues.contains(fallback))
        return clean(*static_cast<const Config::STRING*>(m_configValues.at(fallback)->data()));
    return "";
}

bool CConfigManager::deviceConfigExplicitlySet(const std::string& dev, const std::string& field) {
    return findDeviceValue(normalizeDeviceName(dev), field) != nullptr;
}

bool CConfigManager::deviceConfigExists(const std::string& dev) {
    return m_deviceConfigs.contains(normalizeDeviceName(dev));
}

SConfigOptionReply CConfigManager::getConfigValue(const std::string& s) {

    if (m_configValues.contains(s)) {

        auto& cv = m_configValues[s];

        m_configPtrMap[s] = cv->data();

        return SConfigOptionReply{.dataptr = cc<void* const*>(&m_configPtrMap.at(s)), .type = cv->underlying(), .setByUser = cv->setByUser()};
    }

    // try replacing all . with : unless col.
    std::string s2 = s;

    for (size_t i = 0; i < s2.length(); ++i) {
        if (s2[i] != ':')
            continue;

        if (i <= 3) {
            s2[i] = '.';
            continue;
        }

        if (s2[i - 1] == 'l' && s2[i - 2] == 'o' && s2[i - 3] == 'c')
            continue;

        s2[i] = '.';
    }

    if (!m_configValues.contains(s2))
        return SConfigOptionReply{.dataptr = nullptr};

    auto& cv = m_configValues[s2];

    m_configPtrMap[s2] = cv->data();

    return SConfigOptionReply{.dataptr = cc<void* const*>(&m_configPtrMap.at(s2)), .type = cv->underlying(), .setByUser = cv->setByUser()};
}

std::string CConfigManager::getMainConfigPath() {
    return m_mainConfigPath;
}

std::string CConfigManager::getErrors() {
    std::string errStr;
    for (const auto& e : m_errors) {
        errStr += e + "\n";
    }

    if (!errStr.empty())
        errStr.pop_back();

    return errStr;
}

std::string CConfigManager::getConfigString() {
    return "FIXME:";
}

std::string CConfigManager::currentConfigPath() {
    return m_mainConfigPath;
}

const std::vector<std::string>& CConfigManager::getConfigPaths() {
    return m_configPaths;
}

std::expected<void, std::string> CConfigManager::generateDefaultConfig(const std::filesystem::path&, bool safeMode) {
    // FIXME:
}

void CConfigManager::handlePluginLoads() {
    // FIXME:
}

bool CConfigManager::configVerifPassed() {
    return m_lastConfigVerificationWasSuccessful;
}