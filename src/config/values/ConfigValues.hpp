#pragma once

#include <vector>
#include <limits>
#include <expected>
#include <format>

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/Color.hpp"

#include "./types/IValue.hpp"
#include "./types/BoolValue.hpp"
#include "./types/ColorValue.hpp"
#include "./types/CssGapValue.hpp"
#include "./types/FloatValue.hpp"
#include "./types/FontWeightValue.hpp"
#include "./types/GradientValue.hpp"
#include "./types/IntValue.hpp"
#include "./types/StringValue.hpp"
#include "./types/Vec2Value.hpp"

namespace Config::Values {

    inline auto vec2Range(float minX, float minY, float maxX, float maxY) {
        return [=](const Config::VEC2& v) -> std::expected<void, std::string> {
            if (v.x < minX || v.x > maxX || v.y < minY || v.y > maxY)
                return std::unexpected(std::format("value out of range [{}, {}] - [{}, {}]", minX, minY, maxX, maxY));
            return {};
        };
    }

    inline auto strChoice(std::vector<std::string> rawVec) {
        return [vec = std::move(rawVec)](const Config::STRING& v) -> std::expected<void, std::string> {
            if (!std::ranges::contains(vec, v)) {
                std::string allowed = "";
                for (const auto& e : vec) {
                    allowed += std::format("\"{}\", ", e);
                }

                allowed = allowed.substr(0, allowed.size() - 2);

                return std::unexpected(std::format("bad value \"{}\", allowed values are: {}", v, allowed));
            }
            return {};
        };
    }

#define MS makeShared

    using Bool       = CBoolValue;
    using Color      = CColorValue;
    using CssGap     = CCssGapValue;
    using Float      = CFloatValue;
    using FontWeight = CFontWeightValue;
    using Gradient   = CGradientValue;
    using Int        = CIntValue;
    using String     = CStringValue;
    using Vec2       = CVec2Value;

    using OptionMap = std::unordered_map<std::string, Config::INTEGER>;

    inline const std::vector<SP<IValue>> CONFIG_VALUES = {

        /*
         * general:
         */

        MS<Int>("general:border_size", "size of the border around windows", 1, 0, 20),
        MS<CssGap>("general:gaps_in", "gaps between windows", 5),
        MS<CssGap>("general:gaps_out", "gaps between windows and monitor edges", 20),
        MS<CssGap>("general:float_gaps", "gaps between windows and monitor edges for floating windows", 0),
        MS<Int>("general:gaps_workspaces", "gaps between workspaces. Stacks with gaps_out.", 0, 0, 100),
        MS<Gradient>("general:col.inactive_border", "border color for inactive windows", CHyprColor{0xff444444}),
        MS<Gradient>("general:col.active_border", "border color for the active window", CHyprColor{0xffffffff}),
        MS<Gradient>("general:col.nogroup_border", "inactive border color for window that cannot be added to a group", CHyprColor{0xffffaaff}),
        MS<Gradient>("general:col.nogroup_border_active", "active border color for window that cannot be added to a group", CHyprColor{0xffff00ff}),
        MS<String>("general:layout", "which layout to use. [dwindle/master]", "dwindle"),
        MS<Bool>("general:no_focus_fallback", "if true, will not fall back to the next available window when moving focus in a direction where no window was found", false),
        MS<Bool>("general:resize_on_border", "enables resizing windows by clicking and dragging on borders and gaps", false),
        MS<Int>("general:extend_border_grab_area", "extends the area around the border where you can click and drag on, only used when general:resize_on_border is on.", 15, 0,
                100),
        MS<Bool>("general:hover_icon_on_border", "show a cursor icon when hovering over borders, only used when general:resize_on_border is on.", true),
        MS<Bool>("general:allow_tearing", "master switch for allowing tearing to occur.", false),
        MS<Int>("general:resize_corner", "force floating windows to use a specific corner when being resized (1-4 going clockwise from top left, 0 to disable)", 0, 0, 4,
                OptionMap{{"disable", 0}, {"top_left", 1}, {"top_right", 2}, {"bottom_right", 3}, {"bottom_left", 4}}),
        MS<Bool>("general:snap:enabled", "enable snapping for floating windows", false),
        MS<Int>("general:snap:window_gap", "minimum gap in pixels between windows before snapping", 10, 0, 100),
        MS<Int>("general:snap:monitor_gap", "minimum gap in pixels between window and monitor edges before snapping", 10, 0, 100),
        MS<Bool>("general:snap:border_overlap", "if true, windows snap such that only one border's worth of space is between them", false),
        MS<Bool>("general:snap:respect_gaps", "if true, snapping will respect gaps between windows", false),
        MS<Bool>("general:modal_parent_blocking", "if true, parent windows of modals will not be interactive.", true),
        MS<String>("general:locale", "overrides the system locale", ""),

        /*
         * decoration:
         */

        MS<Int>("decoration:rounding", "rounded corners' radius (in layout px)", 0, 0, 20),
        MS<Float>("decoration:rounding_power", "rounding power of corners (2 is a circle)", 2, 2, 10),
        MS<Float>("decoration:active_opacity", "opacity of active windows.", 1, 0, 1),
        MS<Float>("decoration:inactive_opacity", "opacity of inactive windows.", 1, 0, 1),
        MS<Float>("decoration:fullscreen_opacity", "opacity of fullscreen windows.", 1, 0, 1),
        MS<Bool>("decoration:shadow:enabled", "enable drop shadows on windows", true),
        MS<Int>("decoration:shadow:range", "Shadow range (size) in layout px", 4, 0, 100),
        MS<Int>("decoration:shadow:render_power", "in what power to render the falloff (more power, the faster the falloff)", 3, 1, 4),
        MS<Bool>("decoration:shadow:sharp", "whether the shadow should be sharp or not.", false),
        MS<Bool>("decoration:shadow:ignore_window", "if true, the shadow will not be rendered behind the window itself, only around it.", true),
        MS<Color>("decoration:shadow:color", "shadow's color. Alpha dictates shadow's opacity.", 0xee1a1a1a),
        MS<Color>("decoration:shadow:color_inactive", "inactive shadow color. (if not set, will fall back to col.shadow)", -1),
        MS<Vec2>("decoration:shadow:offset", "shadow's rendering offset.", Config::VEC2{}, vec2Range(-250, -250, 250, 250)),
        MS<Float>("decoration:shadow:scale", "shadow's scale.", 1, 0, 1),
        MS<Bool>("decoration:glow:enabled", "enable inner glow on windows", false),
        MS<Int>("decoration:glow:range", "glow range (size) in layout px", 10, 0, 100),
        MS<Int>("decoration:glow:render_power", "in what power to render the falloff (more power, the faster the falloff)", 3, 1, 4),
        MS<Color>("decoration:glow:color", "glow's color. Alpha dictates glow's opacity.", 0xee33ccff),
        MS<Color>("decoration:glow:color_inactive", "inactive glow color. (if not set, will fall back to decoration:glow:color)", 0x0033ccff),
        MS<Bool>("decoration:dim_modal", "enables dimming of parents of modal windows", true),
        MS<Bool>("decoration:dim_inactive", "enables dimming of inactive windows", false),
        MS<Float>("decoration:dim_strength", "how much inactive windows should be dimmed", 0.5, 0, 1),
        MS<Float>("decoration:dim_special", "how much to dim the rest of the screen by when a special workspace is open.", 0.2, 0, 1),
        MS<Float>("decoration:dim_around", "how much the dimaround window rule should dim by.", 0.4, 0, 1),
        MS<String>("decoration:screen_shader", "a path to a custom shader to be applied at the end of rendering.", STRVAL_EMPTY),
        MS<Bool>("decoration:border_part_of_window", "whether the border should be treated as a part of the window.", true),

        /*
         * blur:
         */

        MS<Bool>("decoration:blur:enabled", "enable kawase window background blur", true),
        MS<Int>("decoration:blur:size", "blur size (distance)", 8, 0, 100),
        MS<Int>("decoration:blur:passes", "the amount of passes to perform", 1, 0, 10),
        MS<Bool>("decoration:blur:ignore_opacity", "make the blur layer ignore the opacity of the window", true),
        MS<Bool>("decoration:blur:new_optimizations", "whether to enable further optimizations to the blur.", true),
        MS<Bool>("decoration:blur:xray", "if enabled, floating windows will ignore tiled windows in their blur.", false),
        MS<Float>("decoration:blur:noise", "how much noise to apply.", 0.0117, 0, 1),
        MS<Float>("decoration:blur:contrast", "contrast modulation for blur.", 0.8916, 0, 2),
        MS<Float>("decoration:blur:brightness", "brightness modulation for blur.", 1, 0, 2),
        MS<Float>("decoration:blur:vibrancy", "Increase saturation of blurred colors.", 0.1696, 0, 1),
        MS<Float>("decoration:blur:vibrancy_darkness", "How strong the effect of vibrancy is on dark areas.", 0, 0, 1),
        MS<Bool>("decoration:blur:special", "whether to blur behind the special workspace (note: expensive)", false),
        MS<Bool>("decoration:blur:popups", "whether to blur popups (e.g. right-click menus)", false),
        MS<Float>("decoration:blur:popups_ignorealpha", "works like ignorealpha in layer rules. If pixel opacity is below set value, will not blur.", 0.2, 0, 1),
        MS<Bool>("decoration:blur:input_methods", "whether to blur input methods (e.g. fcitx5)", false),
        MS<Float>("decoration:blur:input_methods_ignorealpha", "works like ignorealpha in layer rules. If pixel opacity is below set value, will not blur.", 0.2, 0, 1),

        /*
         * animations:
         */

        MS<Bool>("animations:enabled", "enable animations", true),
        MS<Bool>("animations:workspace_wraparound", "changes the direction of slide animations between the first and last workspaces", false),

        /*
         * input:
         */

        MS<String>("input:kb_model", "Appropriate XKB keymap parameter.", STRVAL_EMPTY),
        MS<String>("input:kb_layout", "Appropriate XKB keymap parameter", "us"),
        MS<String>("input:kb_variant", "Appropriate XKB keymap parameter", STRVAL_EMPTY),
        MS<String>("input:kb_options", "Appropriate XKB keymap parameter", STRVAL_EMPTY),
        MS<String>("input:kb_rules", "Appropriate XKB keymap parameter", STRVAL_EMPTY),
        MS<String>("input:kb_file", "Appropriate XKB keymap file", STRVAL_EMPTY),
        MS<Bool>("input:numlock_by_default", "Engage numlock by default.", false),
        MS<Bool>("input:resolve_binds_by_sym", "Determines how keybinds act when multiple layouts are used.", false),
        MS<Int>("input:repeat_rate", "The repeat rate for held-down keys, in repeats per second.", 25, 0, 200),
        MS<Int>("input:repeat_delay", "Delay before a held-down key is repeated, in milliseconds.", 600, 0, 2000),
        MS<Float>("input:sensitivity", "Sets the mouse input sensitivity. Value is clamped to the range -1.0 to 1.0.", 0, -1, 1),
        MS<String>("input:accel_profile", "Sets the cursor acceleration profile. [adaptive/flat/custom]", STRVAL_EMPTY, strChoice({"adaptive", "flat", "custom"})),
        MS<Bool>("input:force_no_accel", "Force no cursor acceleration.", false),
        MS<Int>("input:rotation", "Sets the rotation of a device in degrees clockwise. Value is clamped to the range 0 to 359.", 0, 0, 359),
        MS<Bool>("input:left_handed", "Switches RMB and LMB", false),
        MS<String>("input:scroll_points", "Sets the scroll acceleration profile, when accel_profile is set to custom.", STRVAL_EMPTY),
        MS<String>("input:scroll_method", "Sets the scroll method. [2fg/edge/on_button_down/no_scroll]", STRVAL_EMPTY, strChoice({"2fg", "edge", "on_button_down", "no_scroll"})),
        MS<Int>("input:scroll_button", "Sets the scroll button. 0 means default.", 0, 0, 300),
        MS<Bool>("input:scroll_button_lock", "If the scroll button lock is enabled, the button does not need to be held down.", false),
        MS<Float>("input:scroll_factor", "Multiplier added to scroll movement for external mice.", 1, 0, 2),
        MS<Bool>("input:natural_scroll", "Inverts scrolling direction.", false),
        MS<Int>("input:follow_mouse", "Specify if and how cursor movement should affect window focus.", 1, 0, 3,
                OptionMap{{"disabled", 0}, {"follow", 1}, {"detached", 2}, {"separate", 3}}),
        MS<Float>("input:follow_mouse_threshold", "The smallest distance in logical pixels the mouse needs to travel for the window under it to get focused.", 0),
        MS<Int>("input:focus_on_close", "Controls the window focus behavior when a window is closed.", 0, 0, 2, OptionMap{{"next", 0}, {"cursor", 1}, {"mru", 2}}),
        MS<Bool>("input:mouse_refocus", "if disabled, mouse focus won't switch to the hovered window unless the mouse crosses a window boundary when follow_mouse=1.", true),
        MS<Int>("input:float_switch_override_focus",
                "If enabled (1 or 2), focus will change to the window under the cursor when changing from tiled-to-floating and vice versa. If 2, focus will also follow mouse on "
                "float-to-float switches.",
                1, 0, 2),
        MS<Bool>("input:special_fallthrough", "if enabled, having only floating windows in the special workspace will not block focusing windows in the regular workspace.", false),
        MS<Int>("input:off_window_axis_events", "How to handle axis events around a focused window.", 1, 0, 3, OptionMap{{"ignore", 0}, {"send", 1}, {"clamp", 2}, {"warp", 3}}),
        MS<Int>("input:emulate_discrete_scroll", "Emulates discrete scrolling from high resolution scrolling events.", 1, 0, 2,
                OptionMap{{"disable", 0}, {"non_standard", 1}, {"force_all", 2}}),

        /*
         * input:touchpad:
         */

        MS<Bool>("input:touchpad:disable_while_typing", "Disable the touchpad while typing.", true),
        MS<Bool>("input:touchpad:natural_scroll", "Inverts scrolling direction.", false),
        MS<Float>("input:touchpad:scroll_factor", "Multiplier applied to the amount of scroll movement.", 1, 0, 2),
        MS<Bool>("input:touchpad:middle_button_emulation", "Sending LMB and RMB simultaneously will be interpreted as a middle click.", false),
        MS<String>("input:touchpad:tap_button_map", "Sets the tap button mapping for touchpad button emulation. [lrm/lmr]", STRVAL_EMPTY, strChoice({"lrm", "lmr"})),
        MS<Bool>("input:touchpad:clickfinger_behavior", "Button presses with 1, 2, or 3 fingers will be mapped to LMB, RMB, and MMB respectively.", false),
        MS<Bool>("input:touchpad:tap-to-click", "Tapping on the touchpad with 1, 2, or 3 fingers will send LMB, RMB, and MMB respectively.", true),
        MS<Int>("input:touchpad:drag_lock", "When enabled, lifting the finger off while dragging will not drop the dragged item.", 0, 0, 2),
        MS<Bool>("input:touchpad:tap-and-drag", "Sets the tap and drag mode for the touchpad", true),
        MS<Bool>("input:touchpad:flip_x", "Inverts the horizontal movement of the touchpad", false),
        MS<Bool>("input:touchpad:flip_y", "Inverts the vertical movement of the touchpad", false),
        MS<Int>("input:touchpad:drag_3fg", "Whether to use 3 or 4 finger drag.", 0, 0, 2, OptionMap{{"disable", 0}, {"3_finger", 1}, {"4_finger", 2}}),

        /*
         * input:touchdevice:
         */

        MS<Int>("input:touchdevice:transform", "Transform the input from touchdevices.", 0, 0, 6),
        MS<String>("input:touchdevice:output", "The monitor to bind touch devices.", "[[Auto]]"),
        MS<Bool>("input:touchdevice:enabled", "Whether input is enabled for touch devices.", true),

        /*
         * input:virtualkeyboard:
         */

        MS<Int>("input:virtualkeyboard:share_states", "Unify key down states and modifier states with other keyboards.", 2, 0, 2,
                OptionMap{{"disable", 0}, {"enable", 1}, {"only_non_ime", 2}}),
        MS<Bool>("input:virtualkeyboard:release_pressed_on_close", "Release all pressed keys by virtual keyboard on close.", false),

        /*
         * input:tablet:
         */

        MS<Int>("input:tablet:transform", "transform the input from tablets.", 0, 0, 6),
        MS<String>("input:tablet:output", "the monitor to bind tablets.", STRVAL_EMPTY),
        MS<Vec2>("input:tablet:region_position", "position of the mapped region in monitor layout.", Config::VEC2{}, vec2Range(-20000, -20000, 20000, 20000)),
        MS<Bool>("input:tablet:absolute_region_position", "whether to treat the region_position as an absolute position in monitor layout.", false),
        MS<Vec2>("input:tablet:region_size", "size of the mapped region.", Config::VEC2{}, vec2Range(-100, -100, 4000, 4000)),
        MS<Bool>("input:tablet:relative_input", "whether the input should be relative", false),
        MS<Bool>("input:tablet:left_handed", "if enabled, the tablet will be rotated 180 degrees", false),
        MS<Vec2>("input:tablet:active_area_size", "size of tablet's active area in mm", Config::VEC2{}, vec2Range(0, 0, 500, 500)),
        MS<Vec2>("input:tablet:active_area_position", "position of the active area in mm", Config::VEC2{}, vec2Range(0, 0, 500, 500)),

        /*
         * gestures:
         */

        MS<Int>("gestures:workspace_swipe_distance", "in px, the distance of the touchpad gesture", 300, 0, 2000),
        MS<Bool>("gestures:workspace_swipe_touch", "enable workspace swiping from the edge of a touchscreen", false),
        MS<Bool>("gestures:workspace_swipe_invert", "invert the direction (touchpad only)", true),
        MS<Bool>("gestures:workspace_swipe_touch_invert", "invert the direction (touchscreen only)", false),
        MS<Int>("gestures:workspace_swipe_min_speed_to_force", "minimum speed in px per timepoint to force the change ignoring cancel_ratio.", 30, 0, 200),
        MS<Float>("gestures:workspace_swipe_cancel_ratio", "how much the swipe has to proceed in order to commence it.", 0.5, 0, 1),
        MS<Bool>("gestures:workspace_swipe_create_new", "whether a swipe right on the last workspace should create a new one.", true),
        MS<Bool>("gestures:workspace_swipe_direction_lock", "if enabled, switching direction will be locked when you swipe past the direction_lock_threshold.", true),
        MS<Int>("gestures:workspace_swipe_direction_lock_threshold", "in px, the distance to swipe before direction lock activates.", 10, 0, 200),
        MS<Bool>("gestures:workspace_swipe_forever", "if enabled, swiping will not clamp at the neighboring workspaces but continue to the further ones.", false),
        MS<Bool>("gestures:workspace_swipe_use_r", "if enabled, swiping will use the r prefix instead of the m prefix for finding workspaces.", false),
        MS<Int>("gestures:close_max_timeout", "Timeout for closing windows with the close gesture, in ms.", 1000, 10, 2000),

        /*
         * group:
         */

        MS<Bool>("group:insert_after_current", "whether new windows in a group spawn after current or at group tail", true),
        MS<Bool>("group:focus_removed_window", "whether Hyprland should focus on the window that has just been moved out of the group", true),
        MS<Bool>("group:merge_groups_on_drag", "whether window groups can be dragged into other groups", true),
        MS<Bool>("group:merge_groups_on_groupbar", "whether one group will be merged with another when dragged into its groupbar", true),
        MS<Gradient>("group:col.border_active", "active group border color", CHyprColor{0x66ffff00}),
        MS<Gradient>("group:col.border_inactive", "inactive group border color", CHyprColor{0x66777700}),
        MS<Gradient>("group:col.border_locked_inactive", "inactive locked group border color", CHyprColor{0x66ff5500}),
        MS<Gradient>("group:col.border_locked_active", "active locked group border color", CHyprColor{0x66775500}),
        MS<Bool>("group:auto_group", "automatically group new windows", true),
        MS<Int>("group:drag_into_group", "whether dragging a window into a unlocked group will merge them.", 1, 0, 2,
                OptionMap{{"disabled", 0}, {"enabled", 1}, {"only when dragging into the groupbar", 2}}),
        MS<Bool>("group:merge_floated_into_tiled_on_groupbar", "whether dragging a floating window into a tiled window groupbar will merge them", false),
        MS<Bool>("group:group_on_movetoworkspace", "whether using movetoworkspace[silent] will merge the window into the workspace's solitary unlocked group", false),

        /*
         * group:groupbar:
         */

        MS<Bool>("group:groupbar:enabled", "enables groupbars", true),
        MS<String>("group:groupbar:font_family", "font used to display groupbar titles", "[[EMPTY]]"),
        MS<FontWeight>("group:groupbar:font_weight_active", "weight of the font used to display active groupbar titles"),
        MS<FontWeight>("group:groupbar:font_weight_inactive", "weight of the font used to display inactive groupbar titles"),
        MS<Int>("group:groupbar:font_size", "font size of groupbar title", 8, 2, 64),
        MS<Bool>("group:groupbar:gradients", "enables gradients", false),
        MS<Int>("group:groupbar:height", "height of the groupbar", 14, 1, 64),
        MS<Int>("group:groupbar:indicator_gap", "height of the gap between the groupbar indicator and title", 0, 0, 64),
        MS<Int>("group:groupbar:indicator_height", "height of the groupbar indicator", 3, 1, 64),
        MS<Bool>("group:groupbar:stacked", "render the groupbar as a vertical stack", false),
        MS<Int>("group:groupbar:priority", "sets the decoration priority for groupbars", 3, 0, 6),
        MS<Bool>("group:groupbar:render_titles", "whether to render titles in the group bar decoration", true),
        MS<Bool>("group:groupbar:scrolling", "whether scrolling in the groupbar changes group active window", true),
        MS<Int>("group:groupbar:rounding", "how much to round the groupbar", 1, 0, 20),
        MS<Float>("group:groupbar:rounding_power", "rounding power of groupbar corners (2 is a circle)", 2, 2, 10),
        MS<Int>("group:groupbar:gradient_rounding", "how much to round the groupbar gradient", 2, 0, 20),
        MS<Float>("group:groupbar:gradient_rounding_power", "rounding power of groupbar gradient corners (2 is a circle)", 2, 2, 10),
        MS<Bool>("group:groupbar:round_only_edges", "if yes, will only round at the groupbar edges", true),
        MS<Bool>("group:groupbar:gradient_round_only_edges", "if yes, will only round at the groupbar gradient edges", true),
        MS<Color>("group:groupbar:text_color", "color for window titles in the groupbar", 0xffffffff),
        MS<Color>("group:groupbar:text_color_inactive", "color for inactive windows' titles in the groupbar", -1),
        MS<Color>("group:groupbar:text_color_locked_active", "color for the active window's title in a locked group", -1),
        MS<Color>("group:groupbar:text_color_locked_inactive", "color for inactive windows' titles in locked groups", -1),
        MS<Gradient>("group:groupbar:col.active", "active group border color", 0x66ffff00),
        MS<Gradient>("group:groupbar:col.inactive", "inactive (out of focus) group border color", 0x66777700),
        MS<Gradient>("group:groupbar:col.locked_active", "active locked group border color", 0x66ff5500),
        MS<Gradient>("group:groupbar:col.locked_inactive", "inactive locked group border color", 0x66775500),
        MS<Int>("group:groupbar:gaps_out", "gap between gradients and window", 2, 0, 20),
        MS<Int>("group:groupbar:gaps_in", "gap between gradients", 2, 0, 20),
        MS<Bool>("group:groupbar:keep_upper_gap", "keep an upper gap above gradient", true),
        MS<Int>("group:groupbar:text_offset", "set an offset for a text", 0, -20, 20),
        MS<Int>("group:groupbar:text_padding", "set horizontal padding for a text", 0, 0, 22),
        MS<Bool>("group:groupbar:blur", "enable background blur for groupbars", false),

        /*
         * misc:
         */

        MS<Bool>("misc:disable_hyprland_logo", "disables the random Hyprland logo / anime girl background. :(", false),
        MS<Bool>("misc:disable_splash_rendering", "disables the Hyprland splash rendering.", false),
        MS<Color>("misc:col.splash", "Changes the color of the splash text.", 0x55ffffff),
        MS<String>("misc:font_family", "Set the global default font to render the text.", "Sans"),
        MS<String>("misc:splash_font_family", "Changes the font used to render the splash text.", "[[EMPTY]]"),
        MS<Int>("misc:force_default_wallpaper", "Force any of the 3 default wallpapers. [-1/0/1/2]", -1, -1, 2),
        MS<Bool>("misc:vfr", "controls the VFR status of Hyprland.", true),
        MS<Int>("misc:vrr", "controls the VRR (Adaptive Sync) of your monitors", 0, 0, 3, OptionMap{{"off", 0}, {"on", 1}, {"fullscreen", 2}, {"fullscreen_game", 3}}),
        MS<Bool>("misc:mouse_move_enables_dpms", "If DPMS is set to off, wake up the monitors if the mouse moves", false),
        MS<Bool>("misc:key_press_enables_dpms", "If DPMS is set to off, wake up the monitors if a key is pressed.", false),
        MS<Bool>("misc:name_vk_after_proc", "Name virtual keyboards after the processes that create them.", true),
        MS<Bool>("misc:always_follow_on_dnd", "Will make mouse focus follow the mouse when drag and dropping.", true),
        MS<Bool>("misc:layers_hog_keyboard_focus", "If true, will make keyboard-interactive layers keep their focus on mouse move.", true),
        MS<Bool>("misc:animate_manual_resizes", "If true, will animate manual window resizes/moves", false),
        MS<Bool>("misc:animate_mouse_windowdragging", "If true, will animate windows being dragged by mouse.", false),
        MS<Bool>("misc:disable_autoreload", "If true, the config will not reload automatically on save.", false),
        MS<Bool>("misc:enable_swallow", "Enable window swallowing", false),
        MS<String>("misc:swallow_regex", "The class regex to be used for windows that should be swallowed.", STRVAL_EMPTY),
        MS<String>("misc:swallow_exception_regex", "The title regex to be used for windows that should not be swallowed.", STRVAL_EMPTY),
        MS<Bool>("misc:focus_on_activate", "Whether Hyprland should focus an app that requests to be focused.", false),
        MS<Bool>("misc:mouse_move_focuses_monitor", "Whether mouse moving into a different monitor should focus it", true),
        MS<Bool>("misc:allow_session_lock_restore", "if true, will allow you to restart a lockscreen app in case it crashes.", false),
        MS<Bool>("misc:session_lock_xray", "keep rendering workspaces below your lockscreen", false),
        MS<Color>("misc:background_color", "change the background color.", 0xff111111),
        MS<Bool>("misc:close_special_on_empty", "close the special workspace if the last window is removed", true),
        MS<Int>("misc:on_focus_under_fullscreen", "if there is a fullscreen or maximized window, decide whether a tiled window requested to focus should replace it.", 2, 0, 2,
                OptionMap{{"ignore", 0}, {"take_over", 1}, {"exit_fullscreen", 2}}),
        MS<Bool>("misc:exit_window_retains_fullscreen", "if true, closing a fullscreen window makes the next focused window fullscreen", false),
        MS<Int>("misc:initial_workspace_tracking", "if enabled, windows will open on the workspace they were invoked on.", 1, 0, 2),
        MS<Bool>("misc:middle_click_paste", "whether to enable middle-click-paste (aka primary selection)", true),
        MS<Int>("misc:render_unfocused_fps", "the maximum limit for renderunfocused windows' fps in the background", 15, 1, 120),
        MS<Bool>("misc:disable_xdg_env_checks", "disable the warning if XDG environment is externally managed", false),
        MS<Bool>("misc:disable_hyprland_guiutils_check", "disable the warning if hyprland-guiutils is missing", false),
        MS<Bool>("misc:disable_watchdog_warning", "whether to disable the warning about not using start-hyprland.", false),
        MS<Int>("misc:lockdead_screen_delay", "the delay in ms after the lockdead screen appears.", 1000, 0, 5000),
        MS<Bool>("misc:enable_anr_dialog", "whether to enable the ANR (app not responding) dialog when your apps hang", true),
        MS<Int>("misc:anr_missed_pings", "number of missed pings before showing the ANR dialog", 5, 1, 20),
        MS<Bool>("misc:screencopy_force_8b", "forces 8 bit screencopy", true),
        MS<Bool>("misc:disable_scale_notification", "disables notification popup when a monitor fails to set a suitable scale", false),
        MS<Bool>("misc:size_limits_tiled", "whether to apply minsize and maxsize rules to tiled windows", false),

        /*
         * binds:
         */

        MS<Bool>("binds:pass_mouse_when_bound", "if disabled, will not pass the mouse events to apps / dragging windows around if a keybind has been triggered.", false),
        MS<Int>("binds:scroll_event_delay", "in ms, how many ms to wait after a scroll event to allow passing another one for the binds.", 300, 0, 2000),
        MS<Bool>("binds:workspace_back_and_forth", "If enabled, an attempt to switch to the currently focused workspace will instead switch to the previous workspace.", false),
        MS<Bool>("binds:hide_special_on_workspace_change", "If enabled, changing the active workspace will hide the special workspace on the monitor.", false),
        MS<Bool>("binds:allow_workspace_cycles", "If enabled, workspaces don't forget their previous workspace.", false),
        MS<Int>("binds:workspace_center_on", "Whether switching workspaces should center the cursor on the workspace (0) or on the last active window (1)", 1, 0, 1),
        MS<Int>("binds:focus_preferred_method", "sets the preferred focus finding method when using focuswindow/movewindow/etc with a direction.", 0, 0, 1),
        MS<Bool>("binds:ignore_group_lock", "If enabled, dispatchers like moveintogroup, moveoutofgroup and movewindoworgroup will ignore lock per group.", false),
        MS<Bool>("binds:movefocus_cycles_fullscreen", "If enabled, when on a fullscreen window, movefocus will cycle fullscreen.", false),
        MS<Bool>("binds:movefocus_cycles_groupfirst", "If enabled, when in a grouped window, movefocus will cycle windows in the groups first.", false),
        MS<Bool>("binds:disable_keybind_grabbing", "If enabled, apps that request keybinds to be disabled will not be able to do so.", false),
        MS<Bool>("binds:window_direction_monitor_fallback", "If enabled, moving a window or focus over the edge of a monitor with a direction will move it to the next monitor.",
                 true),
        MS<Bool>("binds:allow_pin_fullscreen", "Allows fullscreen to pinned windows, and restore their pinned status afterwards", false),
        MS<Int>("binds:drag_threshold", "Movement threshold in pixels for window dragging and c/g bind flags. 0 to disable.", 0, 0, std::numeric_limits<int>::max()),

        /*
         * xwayland:
         */

        MS<Bool>("xwayland:enabled", "allow running applications using X11", true),
        MS<Bool>("xwayland:use_nearest_neighbor", "uses the nearest neighbor filtering for xwayland apps, making them pixelated rather than blurry", true),
        MS<Bool>("xwayland:force_zero_scaling", "forces a scale of 1 on xwayland windows on scaled displays.", false),
        MS<Bool>("xwayland:create_abstract_socket", "Create the abstract Unix domain socket for XWayland", false),

        /*
         * opengl:
         */

        MS<Bool>("opengl:nvidia_anti_flicker", "reduces flickering on nvidia at the cost of possible frame drops on lower-end GPUs.", true),

        /*
         * render:
         */

        MS<Int>("render:direct_scanout", "Enables direct scanout.", 0, 0, 2, OptionMap{{"disable", 0}, {"enable", 1}, {"auto", 2}}),
        MS<Bool>("render:expand_undersized_textures", "Whether to expand textures that have not yet resized to be larger.", true),
        MS<Bool>("render:xp_mode", "Disable back buffer and bottom layer rendering.", false),
        MS<Int>("render:ctm_animation", "Whether to enable a fade animation for CTM changes.", 2, 0, 2, OptionMap{{"disable", 0}, {"enable", 1}, {"auto", 2}}),
        MS<Int>("render:cm_fs_passthrough", "Passthrough color settings for fullscreen apps when possible", 2, 0, 2, OptionMap{{"disable", 0}, {"always", 1}, {"hdr_only", 2}}),
        MS<Bool>("render:cm_enabled", "Enable Color Management pipelines (requires restart to fully take effect)", true),
        MS<Bool>("render:send_content_type", "Report content type to allow monitor profile autoswitch", true),
        MS<Int>("render:cm_auto_hdr", "Auto-switch to hdr mode when fullscreen app is in hdr", 1, 0, 2, OptionMap{{"disable", 0}, {"hdr", 1}, {"hdredid", 2}}),
        MS<Bool>("render:new_render_scheduling", "enable new render scheduling, which should improve FPS on underpowered devices.", false),
        MS<Int>("render:non_shader_cm", "Enable CM without shader.", 3, 0, 3, OptionMap{{"disable", 0}, {"always", 1}, {"ondemand", 2}, {"ignore", 3}}),
        MS<String>("render:cm_sdr_eotf", "Default transfer function for displaying SDR apps.", "default"),
        MS<Bool>("render:commit_timing_enabled", "Enable commit timing proto. Requires restart", true),
        MS<Bool>("render:icc_vcgt_enabled", "Enable sending VCGT ramps to KMS with ICC profiles", true),
        MS<Bool>("render:use_shader_blur_blend", "Use experimental blurred bg blending", false),
        MS<Int>("render:use_fp16", "Use experimental internal FP16 buffer.", 2, 0, 2, OptionMap{{"disable", 0}, {"enable", 1}, {"auto", 2}}),
        MS<Int>("render:keep_unmodified_copy", "Keep umodified SDR frame copy for sreensharing.", 2, 0, 2, OptionMap{{"disable", 0}, {"enable", 1}, {"auto", 2}}),

        /*
         * cursor:
         */

        MS<Bool>("cursor:invisible", "don't render cursors", false),
        MS<Int>("cursor:no_hardware_cursors", "disables hardware cursors.", 0, 0, 2, OptionMap{{"Disabled", 0}, {"Enabled", 1}, {"Auto", 2}}),
        MS<Int>("cursor:no_break_fs_vrr", "disables scheduling new frames on cursor movement for fullscreen apps with VRR enabled.", 2, 0, 2,
                OptionMap{{"disable", 0}, {"enable", 1}, {"auto", 2}}),
        MS<Int>("cursor:min_refresh_rate", "minimum refresh rate for cursor movement when no_break_fs_vrr is active.", 24, 10, 500),
        MS<Int>("cursor:hotspot_padding", "the padding, in logical px, between screen edges and the cursor", 0, 0, 20),
        MS<Float>("cursor:inactive_timeout", "in seconds, after how many seconds of cursor's inactivity to hide it. Set to 0 for never.", 0, 0, 20),
        MS<Bool>("cursor:no_warps", "if true, will not warp the cursor in many cases", false),
        MS<Bool>("cursor:persistent_warps", "When a window is refocused, the cursor returns to its last position relative to that window.", false),
        MS<Int>("cursor:warp_on_change_workspace", "Move the cursor to the last focused window after changing the workspace.", 0, 0, 2,
                OptionMap{{"disable", 0}, {"enable", 1}, {"force", 2}}),
        MS<Int>("cursor:warp_on_toggle_special", "Move the cursor to the last focused window when toggling a special workspace.", 0, 0, 2,
                OptionMap{{"disable", 0}, {"enable", 1}, {"force", 2}}),
        MS<String>("cursor:default_monitor", "the name of a default monitor for the cursor to be set to on startup", STRVAL_EMPTY),
        MS<Float>("cursor:zoom_factor", "the factor to zoom by around the cursor. 1 means no zoom.", 1, 1, 10),
        MS<Bool>("cursor:zoom_rigid", "whether the zoom should follow the cursor rigidly or loosely", false),
        MS<Bool>("cursor:zoom_disable_aa", "If enabled, when zooming, no antialiasing will be used", false),
        MS<Bool>("cursor:zoom_detached_camera", "Detaches the camera from the mouse when zoomed in", true),
        MS<Bool>("cursor:enable_hyprcursor", "whether to enable hyprcursor support", true),
        MS<Bool>("cursor:hide_on_key_press", "Hides the cursor when you press any key until the mouse is moved.", false),
        MS<Bool>("cursor:hide_on_touch", "Hides the cursor when the last input was a touch input until a mouse input is done.", true),
        MS<Bool>("cursor:hide_on_tablet", "Hides the cursor when the last input was a tablet input until a mouse input is done.", false),
        MS<Int>("cursor:use_cpu_buffer", "Makes HW cursors use a CPU buffer.", 2, 0, 2, OptionMap{{"disable", 0}, {"enable", 1}, {"auto", 2}}),
        MS<Bool>("cursor:sync_gsettings_theme", "sync xcursor theme with gsettings", true),
        MS<Bool>("cursor:warp_back_after_non_mouse_input", "warp the cursor back to where it was after using a non-mouse input to move it.", false),

        /*
         * ecosystem:
         */

        MS<Bool>("ecosystem:no_update_news", "disable the popup that shows up when you update hyprland to a new version.", false),
        MS<Bool>("ecosystem:no_donation_nag", "disable the popup that shows up twice a year encouraging to donate.", false),
        MS<Bool>("ecosystem:enforce_permissions", "whether to enable permission control.", false),

        /*
         * debug:
         */

        MS<Bool>("debug:overlay", "print the debug performance overlay.", false),
        MS<Bool>("debug:damage_blink", "flash damaged areas", false),
        MS<Bool>("debug:gl_debugging", "enable OpenGL debugging and error checking.", false),
        MS<Bool>("debug:disable_logs", "disable logging to a file", true),
        MS<Bool>("debug:disable_time", "disables time logging", true),
        MS<Int>("debug:damage_tracking", "redraw only the needed bits of the display.", 2, 0, 2, OptionMap{{"disable", 0}, {"monitor", 1}, {"full", 2}}),
        MS<Bool>("debug:enable_stdout_logs", "enables logging to stdout", false),
        MS<Int>("debug:manual_crash", "set to 1 and then back to 0 to crash Hyprland.", 0, 0, 1),
        MS<Bool>("debug:suppress_errors", "if true, do not display config file parsing errors.", false),
        MS<Bool>("debug:disable_scale_checks", "disables verification of the scale factors.", false),
        MS<Int>("debug:error_limit", "limits the number of displayed config file parsing errors.", 5, 0, 20),
        MS<Int>("debug:error_position", "sets the position of the error bar.", 0, 0, 1, OptionMap{{"top", 0}, {"bottom", 1}}),
        MS<Bool>("debug:colored_stdout_logs", "enables colors in the stdout logs.", true),
        MS<Bool>("debug:log_damage", "enables logging the damage.", false),
        MS<Bool>("debug:pass", "enables render pass debugging.", false),
        MS<Bool>("debug:full_cm_proto", "claims support for all cm proto features (requires restart)", false),
        MS<Bool>("debug:ds_handle_same_buffer", "Special case for DS with unmodified buffer", true),
        MS<Bool>("debug:ds_handle_same_buffer_fifo", "Special case for DS with unmodified buffer unlocks fifo", true),
        MS<Bool>("debug:fifo_pending_workaround", "Fifo workaround for empty pending list", false),
        MS<Bool>("debug:render_solitary_wo_damage", "Render solitary window with empty damage", false),

        /*
         * layout:
         */

        MS<Vec2>("layout:single_window_aspect_ratio", "If specified, whenever only a single window is open, it will be coerced into the specified aspect ratio.",
                 Config::VEC2{0, 0}, vec2Range(0, 0, 1000, 1000)),
        MS<Float>("layout:single_window_aspect_ratio_tolerance", "Minimum distance for single_window_aspect_ratio to take effect.", 0.1F, 0.F, 1.F),

        /*
         * dwindle:
         */

        MS<Bool>("dwindle:pseudotile", "enable pseudotiling.", false),
        MS<Int>("dwindle:force_split", "force a split direction for new windows", 0, 0, 2, OptionMap{{"follow_mouse", 0}, {"left", 1}, {"right", 2}}),
        MS<Bool>("dwindle:preserve_split", "if enabled, the split will not change regardless of what happens to the container.", false),
        MS<Bool>("dwindle:smart_split", "if enabled, allows a more precise control over the window split direction based on the cursor's position.", false),
        MS<Bool>("dwindle:smart_resizing", "if enabled, resizing direction will be determined by the mouse's position on the window.", true),
        MS<Bool>("dwindle:permanent_direction_override", "if enabled, makes the preselect direction persist.", false),
        MS<Float>("dwindle:special_scale_factor", "specifies the scale factor of windows on the special workspace", 1, 0, 1),
        MS<Float>("dwindle:split_width_multiplier", "specifies the auto-split width multiplier", 1, 0.1F, 3),
        MS<Bool>("dwindle:use_active_for_splits", "whether to prefer the active window or the mouse position for splits", true),
        MS<Float>("dwindle:default_split_ratio", "the default split ratio on window open.", 1, 0.1F, 1.9F),
        MS<Int>("dwindle:split_bias", "specifies which window will receive the split ratio.", 0, 0, 1, OptionMap{{"directional", 0}, {"current", 1}}),
        MS<Bool>("dwindle:precise_mouse_move", "if enabled, bindm movewindow will drop the window more precisely depending on where your mouse is.", false),

        /*
         * master:
         */

        MS<Bool>("master:allow_small_split", "enable adding additional master windows in a horizontal split style", false),
        MS<Float>("master:special_scale_factor", "the scale of the special workspace windows.", 1, 0, 1),
        MS<Float>("master:mfact", "the size as a percentage of the master window.", 0.55, 0, 1),
        MS<String>("master:new_status", "`master`: new window becomes master; `slave`: new windows are added to slave stack; `inherit`: inherit from focused window", "slave"),
        MS<Bool>("master:new_on_top", "whether a newly open window should be on the top of the stack", false),
        MS<String>("master:new_on_active", "`before`, `after`: place new window relative to the focused window; `none`: place new window according to new_on_top.", "none"),
        MS<String>("master:orientation", "default placement of the master area", "left"),
        MS<Int>("master:slave_count_for_center_master", "when using orientation=center, make the master window centered only when at least this many slave windows are open.", 2, 0,
                10),
        MS<String>("master:center_master_fallback", "Set fallback for center master when slaves are less than slave_count_for_center_master", "left"),
        MS<Bool>("master:center_ignores_reserved", "centers the master window on monitor ignoring reserved areas", false),
        MS<Bool>("master:smart_resizing", "if enabled, resizing direction will be determined by the mouse's position on the window.", true),
        MS<Bool>("master:drop_at_cursor", "when enabled, dragging and dropping windows will put them at the cursor position.", true),
        MS<Bool>("master:always_keep_position", "whether to keep the master window in its configured position when there are no slave windows", false),

        /*
         * scrolling:
         */

        MS<Bool>("scrolling:fullscreen_on_one_column", "when enabled, a single column on a workspace will always span the entire screen.", true),
        MS<Float>("scrolling:column_width", "the default width of a column.", 0.5, 0.1, 1.0),
        MS<Int>("scrolling:focus_fit_method", "When a column is focused, what method should be used to bring it into view", 1, 0, 1, OptionMap{{"center", 0}, {"fit", 1}}),
        MS<Bool>("scrolling:follow_focus", "when a window is focused, should the layout move to bring it into view automatically", true),
        MS<Float>("scrolling:follow_min_visible", "when a window is focused, require that at least a given fraction of it is visible for focus to follow", 0.4, 0.0, 1.0),
        MS<String>("scrolling:explicit_column_widths", "A comma-separated list of preconfigured widths for colresize +conf/-conf", "0.333, 0.5, 0.667, 1.0"),
        MS<String>("scrolling:direction", "Direction in which new windows appear and the layout scrolls", "right"),
        MS<Bool>("scrolling:wrap_focus", "Determines if column focus wraps around", true),
        MS<Bool>("scrolling:wrap_swapcol", "Determines if column movement wraps around", true),

        /*
         * experimental:
         */

        MS<Bool>("experimental:wp_cm_1_2", "Allow wp-cm-v1 version 2", false),

        /*
         * quirks:
         */

        MS<Int>("quirks:prefer_hdr", "Prefer HDR mode.", 0, 0, 2, OptionMap{{"disable", 0}, {"enable", 1}, {"gamescope_only", 2}}),
        MS<Bool>("quirks:skip_non_kms_dmabuf_formats", "Do not report dmabuf formats which cannot be imported into KMS", false),
    };

    std::string getAsJson();

#undef MS
};
