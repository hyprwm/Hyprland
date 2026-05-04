#include "LuaBindingsInternal.hpp"

#include "../objects/LuaLayerRule.hpp"
#include "../objects/LuaWindowRule.hpp"

#include "../types/LuaConfigBool.hpp"
#include "../types/LuaConfigCssGap.hpp"
#include "../types/LuaConfigFloat.hpp"
#include "../types/LuaConfigGradient.hpp"
#include "../types/LuaConfigInt.hpp"
#include "../types/LuaConfigString.hpp"
#include "../types/LuaConfigVec2.hpp"

#include "../../supplementary/executor/Executor.hpp"
#include "../../supplementary/propRefresher/PropRefresher.hpp"
#include "../../shared/animation/AnimationTree.hpp"
#include "../../shared/monitor/MonitorRuleManager.hpp"
#include "../../shared/monitor/Parser.hpp"
#include "../../shared/workspace/WorkspaceRuleManager.hpp"

#include "../../../desktop/rule/Engine.hpp"
#include "../../../desktop/rule/layerRule/LayerRule.hpp"
#include "../../../desktop/rule/layerRule/LayerRuleEffectContainer.hpp"
#include "../../../desktop/rule/windowRule/WindowRule.hpp"
#include "../../../desktop/rule/windowRule/WindowRuleEffectContainer.hpp"
#include "../../../layout/LayoutManager.hpp"
#include "../../../layout/supplementary/WorkspaceAlgoMatcher.hpp"
#include "../../../managers/animation/AnimationManager.hpp"
#include "../../../managers/input/InputManager.hpp"
#include "../../../managers/input/trackpad/TrackpadGestures.hpp"
#include "../../../managers/input/trackpad/gestures/CloseGesture.hpp"
#include "../../../managers/input/trackpad/gestures/CursorZoomGesture.hpp"
#include "../../../managers/input/trackpad/gestures/DispatcherGesture.hpp"
#include "../../../managers/input/trackpad/gestures/FloatGesture.hpp"
#include "../../../managers/input/trackpad/gestures/FullscreenGesture.hpp"
#include "../../../managers/input/trackpad/gestures/LuaFunctionGesture.hpp"
#include "../../../managers/input/trackpad/gestures/MoveGesture.hpp"
#include "../../../managers/input/trackpad/gestures/ResizeGesture.hpp"
#include "../../../managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp"
#include "../../../managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp"
#include "../../../managers/permissions/DynamicPermissionManager.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Config;
using namespace Config::Lua;
using namespace Config::Lua::Bindings;
using namespace Hyprutils::Utils;

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

    struct SLayerRuleEffectDesc {
        const char*                       name;
        std::function<ILuaConfigValue*()> factory;
        uint16_t                          effect;
    };

    struct SWorkspaceRuleFieldDesc {
        const char*                                                    name;
        std::function<ILuaConfigValue*()>                              factory;
        std::function<void(ILuaConfigValue*, Config::CWorkspaceRule&)> apply;
    };

    using LE = Desktop::Rule::eLayerRuleEffect;

    inline const SMonitorFieldDesc MONITOR_FIELDS[] = {
        {"mode", []() -> ILuaConfigValue* { return new CLuaConfigString("preferred"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parseMode(*sc<const Config::STRING*>(v->data())); }},
        {"position", []() -> ILuaConfigValue* { return new CLuaConfigString("auto"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parsePosition(*sc<const Config::STRING*>(v->data())); }},
        {"scale", []() -> ILuaConfigValue* { return new CLuaConfigString("auto"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parseScale(*sc<const Config::STRING*>(v->data())); }},
        {"reserved", []() -> ILuaConfigValue* { return new CLuaConfigCssGap(0); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             const auto& gap = *sc<const Config::CCssGapData*>(v->data());
             return p.setReserved(Desktop::CReservedArea(gap.m_top, gap.m_right, gap.m_bottom, gap.m_left));
         }},
        {"reserved_area", []() -> ILuaConfigValue* { return new CLuaConfigCssGap(0); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             const auto& gap = *sc<const Config::CCssGapData*>(v->data());
             return p.setReserved(Desktop::CReservedArea(gap.m_top, gap.m_right, gap.m_bottom, gap.m_left));
         }},
        {"disabled", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             if (*sc<const Config::BOOL*>(v->data()))
                 p.setDisabled();
             return true;
         }},
        {"transform", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 7); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_transform = sc<wl_output_transform>(*sc<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"mirror", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.setMirror(*sc<const Config::STRING*>(v->data()));
             return true;
         }},
        {"bitdepth", []() -> ILuaConfigValue* { return new CLuaConfigInt(8); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_enable10bit = *sc<const Config::INTEGER*>(v->data()) == 10;
             return true;
         }},
        {"cm", []() -> ILuaConfigValue* { return new CLuaConfigString("srgb"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parseCM(*sc<const Config::STRING*>(v->data())); }},
        {"sdr_eotf", []() -> ILuaConfigValue* { return new CLuaConfigString("default"); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrEotf = NTransferFunction::fromString(*sc<const Config::STRING*>(v->data()));
             return true;
         }},
        {"sdrbrightness", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrBrightness = *sc<const Config::FLOAT*>(v->data());
             return true;
         }},
        {"sdrsaturation", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrSaturation = *sc<const Config::FLOAT*>(v->data());
             return true;
         }},
        {"vrr", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 3); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_vrr = sc<int>(*sc<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"icc", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) { return p.parseICC(*sc<const Config::STRING*>(v->data())); }},
        {"supports_wide_color", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, -1, 1); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_supportsWideColor = sc<int>(*sc<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"supports_hdr", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, -1, 1); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_supportsHDR = sc<int>(*sc<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"sdr_min_luminance", []() -> ILuaConfigValue* { return new CLuaConfigFloat(0.2F); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrMinLuminance = *sc<const Config::FLOAT*>(v->data());
             return true;
         }},
        {"sdr_max_luminance", []() -> ILuaConfigValue* { return new CLuaConfigInt(80); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_sdrMaxLuminance = sc<int>(*sc<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"min_luminance", []() -> ILuaConfigValue* { return new CLuaConfigFloat(-1.F); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_minLuminance = *sc<const Config::FLOAT*>(v->data());
             return true;
         }},
        {"max_luminance", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_maxLuminance = sc<int>(*sc<const Config::INTEGER*>(v->data()));
             return true;
         }},
        {"max_avg_luminance", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); },
         [](ILuaConfigValue* v, CMonitorRuleParser& p) {
             p.rule().m_maxAvgLuminance = sc<int>(*sc<const Config::INTEGER*>(v->data()));
             return true;
         }},
    };

    static_assert(sizeof(Internal::WINDOW_RULE_EFFECT_DESCS) / sizeof(Internal::SWindowRuleEffectDesc) == Internal::WE::WINDOW_RULE_EFFECT_LAST_STATIC - 1);

    inline const SLayerRuleEffectDesc LAYER_RULE_EFFECT_DESCS[] = {
        {"no_anim", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_NO_ANIM},
        {"blur", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_BLUR},
        {"blur_popups", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_BLUR_POPUPS},
        {"ignore_alpha", []() -> ILuaConfigValue* { return new CLuaConfigFloat(0.F, 0.F, 1.F); }, LE::LAYER_RULE_EFFECT_IGNORE_ALPHA},
        {"dim_around", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_DIM_AROUND},
        {"xray", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_XRAY},
        {"animation", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, LE::LAYER_RULE_EFFECT_ANIMATION},
        {"order", []() -> ILuaConfigValue* { return new CLuaConfigInt(0); }, LE::LAYER_RULE_EFFECT_ORDER},
        {"above_lock", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 2); }, LE::LAYER_RULE_EFFECT_ABOVE_LOCK},
        {"no_screen_share", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, LE::LAYER_RULE_EFFECT_NO_SCREEN_SHARE},
    };

    static_assert(sizeof(LAYER_RULE_EFFECT_DESCS) / sizeof(SLayerRuleEffectDesc) == LE::LAYER_RULE_EFFECT_LAST_STATIC - 1);

    inline const SWorkspaceRuleFieldDesc WORKSPACE_RULE_FIELDS[] = {
        {"monitor", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_monitor = *sc<const Config::STRING*>(v->data()); }},
        {"default", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_isDefault = *sc<const Config::BOOL*>(v->data()); }},
        {"persistent", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_isPersistent = *sc<const Config::BOOL*>(v->data()); }},
        {"gaps_in", []() -> ILuaConfigValue* { return new CLuaConfigCssGap(5); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_gapsIn = *sc<const Config::CCssGapData*>(v->data()); }},
        {"gaps_out", []() -> ILuaConfigValue* { return new CLuaConfigCssGap(20); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_gapsOut = *sc<const Config::CCssGapData*>(v->data()); }},
        {"float_gaps", []() -> ILuaConfigValue* { return new CLuaConfigCssGap(0); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_floatGaps = *sc<const Config::CCssGapData*>(v->data()); }},
        {"border_size", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_borderSize = *sc<const Config::INTEGER*>(v->data()); }},
        {"no_border", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_noBorder = *sc<const Config::BOOL*>(v->data()); }},
        {"no_rounding", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_noRounding = *sc<const Config::BOOL*>(v->data()); }},
        {"decorate", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_decorate = *sc<const Config::BOOL*>(v->data()); }},
        {"no_shadow", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_noShadow = *sc<const Config::BOOL*>(v->data()); }},
        {"on_created_empty", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_onCreatedEmptyRunCmd = *sc<const Config::STRING*>(v->data()); }},
        {"default_name", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_defaultName = *sc<const Config::STRING*>(v->data()); }},
        {"layout", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_layout = *sc<const Config::STRING*>(v->data()); }},
        {"animation", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); },
         [](ILuaConfigValue* v, Config::CWorkspaceRule& r) { r.m_animationStyle = *sc<const Config::STRING*>(v->data()); }},
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
        {"tap_to_click", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},
        {"tap_and_drag", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},
        {"drag_lock", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 2); }},
        {"left_handed", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"scroll_method", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"scroll_button", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 300); }},
        {"scroll_button_lock", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},
        {"scroll_points", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
        {"scroll_factor", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F, 0.F, 100.F); }},
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
        {"tags", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},
    };

}

static int hlCurve(lua_State* L) {
    CLuaConfigString nameParser("");
    lua_pushvalue(L, 1);
    auto nameErr = nameParser.parse(L);
    lua_pop(L, 1);
    if (nameErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.curve: first argument (name) must be a string: {}", nameErr.message));

    const auto& name = nameParser.parsed();

    if (!lua_istable(L, 2))
        return Internal::configError(L, "hl.curve: second argument must be a table, e.g. { type = \"bezier\", points = { {0, 0}, {1, 1} } }");

    CLuaConfigString typeParser("");
    auto             typeErr = Internal::parseTableField(L, 2, "type", typeParser);
    if (typeErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.curve(\"{}\"): {}", name, typeErr.message));

    const auto& curveType = typeParser.parsed();

    if (curveType == "bezier") {
        lua_getfield(L, 2, "points");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return Internal::configError(L, std::format("hl.curve(\"{}\"): missing or invalid \"points\" field, expected a table of two points", name));
        }
        int pointsIdx = lua_gettop(L);

        if (luaL_len(L, pointsIdx) != 2) {
            lua_pop(L, 1);
            return Internal::configError(L, std::format("hl.curve(\"{}\"): \"points\" must contain exactly 2 points, e.g. {{ {{0, 0}}, {{1, 1}} }}", name));
        }

        float coords[4] = {};
        for (int pt = 1; pt <= 2; pt++) {
            lua_rawgeti(L, pointsIdx, pt);
            if (!lua_istable(L, -1) || luaL_len(L, -1) != 2) {
                lua_pop(L, 2);
                return Internal::configError(L, std::format("hl.curve(\"{}\"): point {} must be a table of 2 numbers, e.g. {{0.25, 0.1}}", name, pt));
            }
            int ptIdx = lua_gettop(L);

            for (int comp = 0; comp < 2; comp++) {
                lua_rawgeti(L, ptIdx, comp + 1);
                CLuaConfigFloat coordParser(0.F, -1.F, 2.F);
                auto            coordErr = coordParser.parse(L);
                lua_pop(L, 1);
                if (coordErr.errorCode != PARSE_ERROR_OK) {
                    lua_pop(L, 2);
                    return Internal::configError(L, std::format("hl.curve(\"{}\"): point {}[{}]: {}", name, pt, comp + 1, coordErr.message));
                }
                coords[((pt - 1) * 2) + comp] = coordParser.parsed();
            }

            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        g_pAnimationManager->addBezierWithName(name, Vector2D(coords[0], coords[1]), Vector2D(coords[2], coords[3]));
    } else if (curveType == "spring") {

        Hyprutils::Animation::SSpringCurve curve;

        {
            CScopeGuard x([L] { lua_pop(L, 1); });

            lua_getfield(L, 2, "stiffness");

            if (!lua_isnumber(L, -1))
                return Internal::configError(L, std::format("hl.curve(\"{}\"): stiffness expects a number", name));

            curve.stiffness = lua_tonumber(L, -1);

            if (curve.stiffness <= 0.5F)
                return Internal::configError(L, std::format("hl.curve(\"{}\"): stiffness expects a number >= 0.5", name));
        }

        {
            CScopeGuard x([L] { lua_pop(L, 1); });

            lua_getfield(L, 2, "dampening");

            if (!lua_isnumber(L, -1))
                return Internal::configError(L, std::format("hl.curve(\"{}\"): dampening expects a number", name));

            curve.damping = lua_tonumber(L, -1);

            if (curve.damping <= 0.5F)
                return Internal::configError(L, std::format("hl.curve(\"{}\"): dampening expects a number >= 0.5", name));
        }

        {
            CScopeGuard x([L] { lua_pop(L, 1); });

            lua_getfield(L, 2, "mass");

            if (!lua_isnumber(L, -1))
                return Internal::configError(L, std::format("hl.curve(\"{}\"): mass expects a number", name));

            curve.mass = lua_tonumber(L, -1);

            if (curve.mass <= 0.5F)
                return Internal::configError(L, std::format("hl.curve(\"{}\"): mass expects a number >= 0.5", name));
        }

        g_pAnimationManager->addSpringWithName(name, curve);
    } else
        return Internal::configError(L, std::format(R"(hl.curve("{}"): unknown curve type "{}", expected "bezier" or "spring")", name, curveType));

    return 0;
}

static int hlAnimation(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, R"(hl.animation: expected a table, e.g. { leaf = "global", enabled = true, speed = 5, bezier = "default" })");

    CLuaConfigString leafParser("");
    auto             leafErr = Internal::parseTableField(L, 1, "leaf", leafParser);
    if (leafErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.animation: {}", leafErr.message));

    const auto leaf = leafParser.parsed();

    if (!Config::animationTree()->nodeExists(leaf))
        return Internal::configError(L, std::format("hl.animation: no such animation leaf \"{}\"", leaf));

    CLuaConfigBool enabledParser(true);
    auto           enabledErr = Internal::parseTableField(L, 1, "enabled", enabledParser);
    if (enabledErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.animation(\"{}\"): {}", leaf, enabledErr.message));

    bool enabled = enabledParser.parsed();

    if (!enabled) {
        Config::animationTree()->setConfigForNode(leaf, false, 1, "default");
        return 0;
    }

    CLuaConfigFloat speedParser(0.F, 0.F, 100.F);
    auto            speedErr = Internal::parseTableField(L, 1, "speed", speedParser);
    if (speedErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.animation(\"{}\"): {}", leaf, speedErr.message));

    float speed = speedParser.parsed();

    if (speed <= 0)
        return Internal::configError(L, std::format("hl.animation(\"{}\"): speed must be greater than 0", leaf));

    std::string curveName;

    if (Internal::hasTableField(L, 1, "bezier")) {
        CLuaConfigString bezierParser("");
        auto             bezierErr = Internal::parseTableField(L, 1, "bezier", bezierParser);
        if (bezierErr.errorCode != PARSE_ERROR_OK)
            return Internal::configError(L, std::format("hl.animation(\"{}\"): {}", leaf, bezierErr.message));

        const auto& bezierName = bezierParser.parsed();

        if (!g_pAnimationManager->bezierExists(bezierName))
            return Internal::configError(L, std::format(R"(hl.animation("{}"): no such bezier "{}")", leaf, bezierName));

        curveName = bezierName;
    } else if (Internal::hasTableField(L, 1, "spring")) {
        CLuaConfigString springParser("");
        auto             springErr = Internal::parseTableField(L, 1, "spring", springParser);
        if (springErr.errorCode != PARSE_ERROR_OK)
            return Internal::configError(L, std::format("hl.animation(\"{}\"): {}", leaf, springErr.message));

        const auto& springName = springParser.parsed();

        if (!g_pAnimationManager->springExists(springName))
            return Internal::configError(L, std::format(R"(hl.animation("{}"): no such spring "{}")", leaf, springName));

        curveName = "spring:" + springName;
    } else
        return Internal::configError(L, std::format(R"(hl.animation("{}"): bezier or spring is required)", leaf));

    std::string style;
    lua_getfield(L, 1, "style");
    if (!lua_isnil(L, -1)) {
        CLuaConfigString styleParser("");
        auto             styleErr = styleParser.parse(L);
        if (styleErr.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return Internal::configError(L, std::format(R"(hl.animation("{}"): field "style": {})", leaf, styleErr.message));
        }
        style = styleParser.parsed();
    }
    lua_pop(L, 1);

    if (!style.empty()) {
        auto err = g_pAnimationManager->styleValidInConfigVar(leaf, style);
        if (!err.empty())
            return Internal::configError(L, std::format("hl.animation(\"{}\"): {}", leaf, err));
    }

    Config::animationTree()->setConfigForNode(leaf, true, speed, curveName, style);
    return 0;
}

static int hlEnv(lua_State* L) {
    auto*            mgr = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    CLuaConfigString nameParser("");
    lua_pushvalue(L, 1);
    auto nameErr = nameParser.parse(L);
    lua_pop(L, 1);
    if (nameErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.env: first argument (name) must be a string: {}", nameErr.message));

    const auto& name = nameParser.parsed();

    if (name.empty())
        return Internal::configError(L, "hl.env: name must not be empty");

    CLuaConfigString valueParser("");
    lua_pushvalue(L, 2);
    auto valueErr = valueParser.parse(L);
    lua_pop(L, 1);
    if (valueErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.env: second argument (value) must be a string: {}", valueErr.message));

    const auto& value = valueParser.parsed();

    if (!mgr->isFirstLaunch()) {
        const auto* ENV = getenv(name.c_str());
        if (ENV && ENV == value)
            return 0;
    }

    setenv(name.c_str(), value.c_str(), 1);

    bool dbus = false;
    if (!lua_isnoneornil(L, 3)) {
        CLuaConfigBool dbusParser(false);
        lua_pushvalue(L, 3);
        auto dbusErr = dbusParser.parse(L);
        lua_pop(L, 1);
        if (dbusErr.errorCode != PARSE_ERROR_OK)
            return Internal::configError(L, std::format("hl.env: third argument (dbus) must be a boolean: {}", dbusErr.message));

        dbus = dbusParser.parsed();
    }

    if (dbus) {
        std::string CMD;
#ifdef USES_SYSTEMD
        CMD = "systemctl --user import-environment '" + name + "' && hash dbus-update-activation-environment 2>/dev/null && ";
#endif
        CMD += "dbus-update-activation-environment --systemd '" + name + "'";
        if (mgr->isFirstLaunch())
            Config::Supplementary::executor()->addExecOnce({CMD, false});
        else
            Config::Supplementary::executor()->spawnRaw(CMD);
    }

    return 0;
}

static int hlPluginLoad(lua_State* L) {
    auto*            mgr = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    CLuaConfigString pathParser("");
    lua_pushvalue(L, 1);
    auto pathErr = pathParser.parse(L);
    lua_pop(L, 1);
    if (pathErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.plugin.load: first argument (path) must be a string: {}", pathErr.message));

    const auto& path = pathParser.parsed();

    if (path.empty())
        return Internal::configError(L, "hl.plugin.load: path must not be empty");

    mgr->m_registeredPlugins.emplace_back(path);
    return 0;
}

static int hlPermission(lua_State* L) {
    auto*       mgr = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    std::string binary;
    std::string typeStr;
    std::string modeStr;

    if (lua_istable(L, 1)) {
        auto b = Internal::tableOptStr(L, 1, "binary");
        if (!b)
            b = Internal::tableOptStr(L, 1, "target");
        auto t = Internal::tableOptStr(L, 1, "type");
        auto m = Internal::tableOptStr(L, 1, "mode");

        if (!b || !t || !m)
            return Internal::configError(L, "hl.permission: expected { binary, type, mode }");

        binary  = *b;
        typeStr = *t;
        modeStr = *m;
    } else {
        binary  = luaL_checkstring(L, 1);
        typeStr = luaL_checkstring(L, 2);
        modeStr = luaL_checkstring(L, 3);
    }

    if (binary.empty())
        return Internal::configError(L, "hl.permission: binary must not be empty");

    eDynamicPermissionType      type = PERMISSION_TYPE_UNKNOWN;
    eDynamicPermissionAllowMode mode = PERMISSION_RULE_ALLOW_MODE_UNKNOWN;

    if (typeStr == "screencopy")
        type = PERMISSION_TYPE_SCREENCOPY;
    else if (typeStr == "cursorpos")
        type = PERMISSION_TYPE_CURSOR_POS;
    else if (typeStr == "plugin")
        type = PERMISSION_TYPE_PLUGIN;
    else if (typeStr == "keyboard" || typeStr == "keeb")
        type = PERMISSION_TYPE_KEYBOARD;

    if (modeStr == "ask")
        mode = PERMISSION_RULE_ALLOW_MODE_ASK;
    else if (modeStr == "allow")
        mode = PERMISSION_RULE_ALLOW_MODE_ALLOW;
    else if (modeStr == "deny")
        mode = PERMISSION_RULE_ALLOW_MODE_DENY;

    if (type == PERMISSION_TYPE_UNKNOWN)
        return Internal::configError(L, "hl.permission: unknown permission type");
    if (mode == PERMISSION_RULE_ALLOW_MODE_UNKNOWN)
        return Internal::configError(L, "hl.permission: unknown permission allow mode");

    if (mgr->isFirstLaunch() && g_pDynamicPermissionManager)
        g_pDynamicPermissionManager->addConfigPermissionRule(binary, type, mode);

    return 0;
}

static int hlWorkspaceRule(lua_State* L) {
    auto* self = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (!lua_istable(L, 1)) {
        self->addError("hl.workspace_rule: argument must be a table");
        return 0;
    }

    const std::string sourceInfo = Internal::getSourceInfo(L);

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

        if (key == "layout_opts") {
            if (!lua_istable(L, -1)) {
                self->addError(std::format("{}: hl.workspace_rule: field 'layout_opts' must be a table", sourceInfo));
                lua_pop(L, 1);
                continue;
            }

            const int optsIdx = lua_gettop(L);
            lua_pushnil(L);
            while (lua_next(L, optsIdx) != 0) {
                if (lua_type(L, -2) != LUA_TSTRING) {
                    self->addError(std::format("{}: hl.workspace_rule: field 'layout_opts' keys must be strings", sourceInfo));
                    lua_pop(L, 1);
                    continue;
                }

                std::string optKey = lua_tostring(L, -2);
                std::string optVal;

                if (lua_type(L, -1) == LUA_TBOOLEAN)
                    optVal = lua_toboolean(L, -1) ? "true" : "false";
                else if (lua_type(L, -1) == LUA_TNUMBER) {
                    if (lua_isinteger(L, -1))
                        optVal = std::to_string(lua_tointeger(L, -1));
                    else
                        optVal = std::to_string(lua_tonumber(L, -1));
                } else if (lua_isstring(L, -1))
                    optVal = lua_tostring(L, -1);
                else {
                    self->addError(std::format("{}: hl.workspace_rule: field 'layout_opts.{}' must be string, bool, or number", sourceInfo, optKey));
                    lua_pop(L, 1);
                    continue;
                }

                wsRule.m_layoutopts[std::move(optKey)] = std::move(optVal);
                lua_pop(L, 1);
            }

            lua_pop(L, 1);
            continue;
        }

        const auto* desc = Internal::findDescByName(WORKSPACE_RULE_FIELDS, key);
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

    Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_MONITOR_STATES | Config::Supplementary::REFRESH_WINDOW_STATES);

    return 0;
}

static int hlGesture(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.gesture: expected a table, e.g. { fingers = 3, direction = \"horizontal\", action = \"workspace\" }");

    CLuaConfigInt fingersParser(0, 2, 9);
    auto          fingersErr = Internal::parseTableField(L, 1, "fingers", fingersParser);
    if (fingersErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.gesture: {}", fingersErr.message));

    size_t           fingerCount = fingersParser.parsed();

    CLuaConfigString dirParser("");
    auto             dirErr = Internal::parseTableField(L, 1, "direction", dirParser);
    if (dirErr.errorCode != PARSE_ERROR_OK)
        return Internal::configError(L, std::format("hl.gesture: {}", dirErr.message));

    const auto direction = g_pTrackpadGestures->dirForString(dirParser.parsed());
    if (direction == TRACKPAD_GESTURE_DIR_NONE)
        return Internal::configError(L, std::format("hl.gesture: invalid direction \"{}\"", dirParser.parsed()));

    int functionRef = LUA_NOREF;

    {
        // check if the action arg is a lua fn, that's fine
        // we can ref that fucker and call him later
        lua_getfield(L, 1, "action");

        if (lua_isfunction(L, -1)) {
            lua_pushvalue(L, -1);
            functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
            Lua::mgr()->registerLuaRef(functionRef);
        }

        lua_pop(L, 1);
    }

    // bitch ass macro because it's kinda long to get these things and it's ugly
#define GET_ACTION_STRING(var, name)                                                                                                                                               \
    std::string var;                                                                                                                                                               \
    lua_getfield(L, 1, name);                                                                                                                                                      \
    if (!lua_isnil(L, -1)) {                                                                                                                                                       \
        CLuaConfigString argParser("");                                                                                                                                            \
        auto             argErr = argParser.parse(L);                                                                                                                              \
        if (argErr.errorCode != PARSE_ERROR_OK) {                                                                                                                                  \
            lua_pop(L, 1);                                                                                                                                                         \
            return Internal::configError(L, std::format("hl.gesture: field \"" name "\": {}", argErr.message));                                                                    \
        }                                                                                                                                                                          \
        var = argParser.parsed();                                                                                                                                                  \
    }                                                                                                                                                                              \
    lua_pop(L, 1);

    GET_ACTION_STRING(zoomLevel, "zoom_level");
    GET_ACTION_STRING(workspaceName, "workspace_name");
    GET_ACTION_STRING(mode, "mode");

#undef GET_ACTION_STRING

    uint32_t modMask = 0;
    lua_getfield(L, 1, "mods");
    if (!lua_isnil(L, -1)) {
        CLuaConfigString modsParser("");
        auto             modsErr = modsParser.parse(L);
        if (modsErr.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return Internal::configError(L, std::format("hl.gesture: field \"mods\": {}", modsErr.message));
        }
        modMask = g_pKeybindManager->stringToModMask(modsParser.parsed());
    }
    lua_pop(L, 1);

    float deltaScale = 1.F;
    lua_getfield(L, 1, "scale");
    if (!lua_isnil(L, -1)) {
        CLuaConfigFloat scaleParser(1.F, 0.1F, 10.F);
        auto            scaleErr = scaleParser.parse(L);
        if (scaleErr.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return Internal::configError(L, std::format("hl.gesture: field \"scale\": {}", scaleErr.message));
        }
        deltaScale = scaleParser.parsed();
    }
    lua_pop(L, 1);

    bool disableInhibit = false;
    lua_getfield(L, 1, "disable_inhibit");
    if (!lua_isnil(L, -1)) {
        CLuaConfigBool disableInhibitParser(false);
        auto           disableInhibitErr = disableInhibitParser.parse(L);
        if (disableInhibitErr.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return Internal::configError(L, std::format("hl.gesture: field \"disable_inhibit\": {}", disableInhibitErr.message));
        }
        disableInhibit = disableInhibitParser.parsed();
    }
    lua_pop(L, 1);

    std::expected<void, std::string> result;

    if (functionRef != LUA_NOREF) {
        // this is a lua fn gesture
        result = g_pTrackpadGestures->addGesture(makeUnique<CLuaFunctionGesture>(functionRef), fingerCount, direction, modMask, deltaScale, disableInhibit);
    } else {
        CLuaConfigString actionParser("");
        auto             actionErr = Internal::parseTableField(L, 1, "action", actionParser);
        if (actionErr.errorCode != PARSE_ERROR_OK)
            return Internal::configError(L, std::format("hl.gesture: {}", actionErr.message));

        const auto& action = actionParser.parsed();

        if (action == "workspace")
            result = g_pTrackpadGestures->addGesture(makeUnique<CWorkspaceSwipeGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
        else if (action == "resize")
            result = g_pTrackpadGestures->addGesture(makeUnique<CResizeTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
        else if (action == "move")
            result = g_pTrackpadGestures->addGesture(makeUnique<CMoveTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
        else if (action == "special")
            result = g_pTrackpadGestures->addGesture(makeUnique<CSpecialWorkspaceGesture>(workspaceName), fingerCount, direction, modMask, deltaScale, disableInhibit);
        else if (action == "close")
            result = g_pTrackpadGestures->addGesture(makeUnique<CCloseTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
        else if (action == "float")
            result = g_pTrackpadGestures->addGesture(makeUnique<CFloatTrackpadGesture>(mode), fingerCount, direction, modMask, deltaScale, disableInhibit);
        else if (action == "fullscreen")
            result = g_pTrackpadGestures->addGesture(makeUnique<CFullscreenTrackpadGesture>(mode), fingerCount, direction, modMask, deltaScale, disableInhibit);
        else if (action == "cursorZoom")
            result = g_pTrackpadGestures->addGesture(makeUnique<CCursorZoomTrackpadGesture>(zoomLevel, mode), fingerCount, direction, modMask, deltaScale, disableInhibit);
        else if (action == "unset")
            result = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale, disableInhibit);
        else
            return Internal::configError(L, std::format("hl.gesture: unknown action \"{}\"", action));
    }

    if (!result)
        return Internal::configError(L, std::format("hl.gesture: {}", result.error()));

    return 0;
}

static int hlConfig(lua_State* L) {
    auto* self = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (!lua_istable(L, 1)) {
        self->addError("hl.config: argument must be a table");
        return 0;
    }

    const std::string                            sourceInfo = Internal::getSourceInfo(L);

    std::function<void(const std::string&, int)> walk = [&](const std::string& prefix, int tableIdx) {
        lua_pushnil(L);
        while (lua_next(L, tableIdx) != 0) {
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

            auto it = self->m_configValues.find(fullKey);

            if (it == self->m_configValues.end() && lua_istable(L, -1))
                walk(fullKey, lua_gettop(L));
            else {
                if (it == self->m_configValues.end())
                    self->addError(std::format("{}: unknown config key '{}'", sourceInfo, fullKey));
                else {
                    const auto err = it->second->parse(L);
                    if (err.errorCode != PARSE_ERROR_OK)
                        self->addError(std::format("{}: error setting '{}': {}", sourceInfo, it->first, err.message));
                    else if (self->isDynamicParse())
                        Supplementary::refresher()->scheduleRefresh(it->second->refreshBits());
                }
            }

            lua_pop(L, 1);
        }
    };

    walk("", 1);
    return 0;
}

static int hlGetConfig(lua_State* L) {
    auto*       self = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    std::string key = luaL_checkstring(L, 1);

    auto        it = self->m_configValues.find(key);
    if (it == self->m_configValues.end()) {
        std::ranges::replace(key, ':', '.');
        it = self->m_configValues.find(key);
    }

    if (it == self->m_configValues.end()) {
        lua_pushnil(L);
        const auto msg = std::format("unknown config key '{}'", key);
        lua_pushstring(L, msg.c_str());
        return 2;
    }

    it->second->push(L);
    return 1;
}

static int hlDevice(lua_State* L) {
    auto* self = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (!lua_istable(L, 1)) {
        self->addError("hl.device: argument must be a table");
        return 0;
    }

    const std::string sourceInfo = Internal::getSourceInfo(L);

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

        const auto* desc = Internal::findDescByName(DEVICE_FIELDS, key);

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

    Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_INPUT_DEVICES);

    return 0;
}

static int hlMonitor(lua_State* L) {
    auto* self = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (!lua_istable(L, 1)) {
        self->addError("hl.monitor: argument must be a table");
        return 0;
    }

    const std::string sourceInfo = Internal::getSourceInfo(L);

    lua_getfield(L, 1, "output");
    if (!lua_isstring(L, -1)) {
        self->addError(std::format("{}: hl.monitor: 'output' field is required and must be a string", sourceInfo));
        lua_pop(L, 1);
        return 0;
    }
    const std::string output = lua_tostring(L, -1);
    lua_pop(L, 1);

    CMonitorRuleParser parser(output);

    const auto         existing = std::ranges::find_if(Config::monitorRuleMgr()->all(), [&output](const auto& rule) { return rule.m_name == output; });
    if (existing != Config::monitorRuleMgr()->all().end())
        parser.rule() = *existing;

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

        const auto* desc = Internal::findDescByName(MONITOR_FIELDS, key);

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

    Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_MONITOR_STATES);

    return 0;
}

static int hlWindowRule(lua_State* L) {
    auto* self = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (!lua_istable(L, 1)) {
        self->addError("hl.window_rule: argument must be a table");
        return 0;
    }

    const std::string sourceInfo = Internal::getSourceInfo(L);

    std::string       name;
    lua_getfield(L, 1, "name");
    if (lua_isstring(L, -1))
        name = lua_tostring(L, -1);
    lua_pop(L, 1);

    bool enabled = true;
    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1))
        enabled = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

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

        const auto* desc = Internal::findDescByName(Internal::WINDOW_RULE_EFFECT_DESCS, key);
        if (!desc) {
            const auto dynamicEffect = Desktop::Rule::windowEffects()->get(key);
            if (!dynamicEffect.has_value()) {
                self->addError(std::format("{}: hl.window_rule: unknown field '{}'", sourceInfo, key));
                lua_pop(L, 1);
                continue;
            }

            auto val = Internal::ruleValueToString(L);
            if (!val)
                self->addError(std::format("{}: hl.window_rule: field '{}': {}", sourceInfo, key, val.error()));
            else {
                auto res = rule->addEffect(*dynamicEffect, *val);
                if (!res)
                    self->addError(std::format("{}: hl.window_rule: field '{}': {}", sourceInfo, key, res.error()));
            }

            lua_pop(L, 1);
            continue;
        }

        auto res = Internal::addWindowRuleEffectFromLua(L, *desc, rule);
        if (!res)
            self->addError(std::format("{}: hl.window_rule: field '{}': {}", sourceInfo, key, res.error()));

        lua_pop(L, 1);
    }

    Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_WINDOW_STATES);

    Objects::CLuaWindowRule::push(L, rule);
    return 1;
}

static int hlLayerRule(lua_State* L) {
    auto* self = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (!lua_istable(L, 1)) {
        self->addError("hl.layer_rule: argument must be a table");
        return 0;
    }

    const std::string sourceInfo = Internal::getSourceInfo(L);

    std::string       name;
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

        const auto* desc = Internal::findDescByName(LAYER_RULE_EFFECT_DESCS, key);
        if (!desc) {
            const auto dynamicEffect = Desktop::Rule::layerEffects()->get(key);
            if (!dynamicEffect.has_value()) {
                self->addError(std::format("{}: hl.layer_rule: unknown field '{}'", sourceInfo, key));
                lua_pop(L, 1);
                continue;
            }

            auto val = Internal::ruleValueToString(L);
            if (!val)
                self->addError(std::format("{}: hl.layer_rule: field '{}': {}", sourceInfo, key, val.error()));
            else {
                auto res = rule->addEffect(*dynamicEffect, *val);
                if (!res)
                    self->addError(std::format("{}: hl.layer_rule: field '{}': {}", sourceInfo, key, res.error()));
            }

            lua_pop(L, 1);
            continue;
        }

        auto val = UP<ILuaConfigValue>(desc->factory());
        auto err = val->parse(L);
        if (err.errorCode != PARSE_ERROR_OK)
            self->addError(std::format("{}: hl.layer_rule: field '{}': {}", sourceInfo, key, err.message));
        else {
            auto str = val->toString();
            auto res = rule->addEffect(desc->effect, str);
            if (!res)
                self->addError(std::format("{}: hl.layer_rule: field '{}': {}", sourceInfo, key, res.error()));
        }

        lua_pop(L, 1);
    }

    Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_RULES);

    Objects::CLuaLayerRule::push(L, rule);
    return 1;
}

void Internal::registerConfigRuleBindings(lua_State* L, CConfigManager* mgr) {
    Internal::setMgrFn(L, mgr, "config", hlConfig);
    Internal::setMgrFn(L, mgr, "get_config", hlGetConfig);
    Internal::setMgrFn(L, mgr, "device", hlDevice);
    Internal::setMgrFn(L, mgr, "monitor", hlMonitor);
    Internal::setMgrFn(L, mgr, "window_rule", hlWindowRule);
    Internal::setMgrFn(L, mgr, "layer_rule", hlLayerRule);
    Internal::setMgrFn(L, mgr, "workspace_rule", hlWorkspaceRule);
    Internal::setMgrFn(L, mgr, "env", hlEnv);
    Internal::setMgrFn(L, mgr, "permission", hlPermission);

    lua_newtable(L);
    Internal::setMgrFn(L, mgr, "load", hlPluginLoad);
    lua_setfield(L, -2, "plugin");

    Internal::setFn(L, "gesture", hlGesture);
    Internal::setFn(L, "curve", hlCurve);
    Internal::setFn(L, "animation", hlAnimation);
}
