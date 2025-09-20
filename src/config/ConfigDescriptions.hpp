#pragma once

#include <climits>
#include "ConfigManager.hpp"

inline static const std::vector<SConfigOptionDescription> CONFIG_OPTIONS = {

    /*
     * general:
     */

    SConfigOptionDescription{
        .value       = "general:border_size",
        .description = "size of the border around windows",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "general:no_border_on_floating",
        .description = "disable borders for floating windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "general:gaps_in",
        .description = "gaps between windows\n\nsupports css style gaps (top, right, bottom, left -> 5 10 15 20)",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"5"},
    },
    SConfigOptionDescription{
        .value       = "general:gaps_out",
        .description = "gaps between windows and monitor edges\n\nsupports css style gaps (top, right, bottom, left -> 5 10 15 20)",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"20"},
    },
    SConfigOptionDescription{
        .value       = "general:float_gaps",
        .description = "gaps between windows and monitor edges for floating windows\n\nsupports css style gaps (top, right, bottom, left -> 5 10 15 20). \n-1 means default "
                       "gaps_in/gaps_out\n0 means no gaps",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"0"},
    },
    SConfigOptionDescription{
        .value       = "general:gaps_workspaces",
        .description = "gaps between workspaces. Stacks with gaps_out.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 100},
    },
    SConfigOptionDescription{
        .value       = "general:col.inactive_border",
        .description = "border color for inactive windows",
        .type        = CONFIG_OPTION_GRADIENT,
        .data        = SConfigOptionDescription::SGradientData{"0xff444444"},
    },
    SConfigOptionDescription{
        .value       = "general:col.active_border",
        .description = "border color for the active window",
        .type        = CONFIG_OPTION_GRADIENT,
        .data        = SConfigOptionDescription::SGradientData{"0xffffffff"},
    },
    SConfigOptionDescription{
        .value       = "general:col.nogroup_border",
        .description = "inactive border color for window that cannot be added to a group (see denywindowfromgroup dispatcher)",
        .type        = CONFIG_OPTION_GRADIENT,
        .data        = SConfigOptionDescription::SGradientData{"0xffffaaff"},
    },
    SConfigOptionDescription{
        .value       = "general:col.nogroup_border_active",
        .description = "active border color for window that cannot be added to a group",
        .type        = CONFIG_OPTION_GRADIENT,
        .data        = SConfigOptionDescription::SGradientData{"0xffff00ff"},
    },
    SConfigOptionDescription{
        .value       = "general:layout",
        .description = "which layout to use. [dwindle/master]",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"dwindle"},
    },
    SConfigOptionDescription{
        .value       = "general:no_focus_fallback",
        .description = "if true, will not fall back to the next available window when moving focus in a direction where no window was found",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "general:resize_on_border",
        .description = "enables resizing windows by clicking and dragging on borders and gaps",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "general:extend_border_grab_area",
        .description = "extends the area around the border where you can click and drag on, only used when general:resize_on_border is on.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{15, 0, 100},
    },
    SConfigOptionDescription{
        .value       = "general:hover_icon_on_border",
        .description = "show a cursor icon when hovering over borders, only used when general:resize_on_border is on.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "general:allow_tearing",
        .description = "master switch for allowing tearing to occur. See the Tearing page.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "general:resize_corner",
        .description = "force floating windows to use a specific corner when being resized (1-4 going clockwise from top left, 0 to disable)",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 4},
    },
    SConfigOptionDescription{
        .value       = "general:snap:enabled",
        .description = "enable snapping for floating windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "general:snap:window_gap",
        .description = "minimum gap in pixels between windows before snapping",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{10, 0, 100},
    },
    SConfigOptionDescription{
        .value       = "general:snap:monitor_gap",
        .description = "minimum gap in pixels between window and monitor edges before snapping",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{10, 0, 100},
    },
    SConfigOptionDescription{
        .value       = "general:snap:border_overlap",
        .description = "if true, windows snap such that only one border's worth of space is between them",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "general:snap:respect_gaps",
        .description = "if true, snapping will respect gaps between windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * decoration:
     */

    SConfigOptionDescription{
        .value       = "decoration:rounding",
        .description = "rounded corners' radius (in layout px)",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "decoration:rounding_power",
        .description = "rounding power of corners (2 is a circle)",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{2, 2, 10},
    },
    SConfigOptionDescription{
        .value       = "decoration:active_opacity",
        .description = "opacity of active windows. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:inactive_opacity",
        .description = "opacity of inactive windows. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:fullscreen_opacity",
        .description = "opacity of fullscreen windows. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:enabled",
        .description = "enable drop shadows on windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:range",
        .description = "Shadow range (size) in layout px",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{4, 0, 100},
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:render_power",
        .description = "in what power to render the falloff (more power, the faster the falloff) [1 - 4]",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{3, 1, 4},
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:sharp",
        .description = "whether the shadow should be sharp or not. Akin to an infinitely high render power.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:ignore_window",
        .description = "if true, the shadow will not be rendered behind the window itself, only around it.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:color",
        .description = "shadow's color. Alpha dictates shadow's opacity.",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{0xee1a1a1a},
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:color_inactive",
        .description = "inactive shadow color. (if not set, will fall back to col.shadow)",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{}, //TODO: UNSET?
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:offset",
        .description = "shadow's rendering offset.",
        .type        = CONFIG_OPTION_VECTOR,
        .data        = SConfigOptionDescription::SVectorData{{}, {-250, -250}, {250, 250}},
    },
    SConfigOptionDescription{
        .value       = "decoration:shadow:scale",
        .description = "shadow's scale. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:dim_modal",
        .description = "enables dimming of parents of modal windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "decoration:dim_inactive",
        .description = "enables dimming of inactive windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "decoration:dim_strength",
        .description = "how much inactive windows should be dimmed [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.5, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:dim_special",
        .description = "how much to dim the rest of the screen by when a special workspace is open. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.2, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:dim_around",
        .description = "how much the dimaround window rule should dim by. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.4, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:screen_shader",
        .description = "screen_shader a path to a custom shader to be applied at the end of rendering. See examples/screenShader.frag for an example.",
        .type        = CONFIG_OPTION_STRING_LONG,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "decoration:border_part_of_window",
        .description = "whether the border should be treated as a part of the window.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },

    /*
     * blur:
     */

    SConfigOptionDescription{
        .value       = "decoration:blur:enabled",
        .description = "enable kawase window background blur",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:size",
        .description = "blur size (distance)",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{8, 0, 100},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:passes",
        .description = "the amount of passes to perform",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 10},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:ignore_opacity",
        .description = "make the blur layer ignore the opacity of the window",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:new_optimizations",
        .description = "whether to enable further optimizations to the blur. Recommended to leave on, as it will massively improve performance.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:xray",
        .description = "if enabled, floating windows will ignore tiled windows in their blur. Only available if blur_new_optimizations is true. Will reduce overhead on floating "
                       "blur significantly.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:noise",
        .description = "how much noise to apply. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.0117, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:contrast",
        .description = "contrast modulation for blur. [0.0 - 2.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.8916, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:brightness",
        .description = "brightness modulation for blur. [0.0 - 2.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.8172, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:vibrancy",
        .description = "Increase saturation of blurred colors. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.1696, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:vibrancy_darkness",
        .description = "How strong the effect of vibrancy is on dark areas . [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:special",
        .description = "whether to blur behind the special workspace (note: expensive)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:popups",
        .description = "whether to blur popups (e.g. right-click menus)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:popups_ignorealpha",
        .description = "works like ignorealpha in layer rules. If pixel opacity is below set value, will not blur. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.2, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:input_methods",
        .description = "whether to blur input methods (e.g. fcitx5)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "decoration:blur:input_methods_ignorealpha",
        .description = "works like ignorealpha in layer rules. If pixel opacity is below set value, will not blur. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.2, 0, 1},
    },

    /*
     * animations:
     */

    SConfigOptionDescription{
        .value       = "animations:enabled",
        .description = "enable animations",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "animations:workspace_wraparound",
        .description = "changes the direction of slide animations between the first and last workspaces",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },

    /*
     * input:
     */

    SConfigOptionDescription{
        .value       = "input:kb_model",
        .description = "Appropriate XKB keymap parameter. See the note below.",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{STRVAL_EMPTY},
    },
    SConfigOptionDescription{
        .value       = "input:kb_layout",
        .description = "Appropriate XKB keymap parameter",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"us"},
    },
    SConfigOptionDescription{
        .value       = "input:kb_variant",
        .description = "Appropriate XKB keymap parameter",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""},
    },
    SConfigOptionDescription{
        .value       = "input:kb_options",
        .description = "Appropriate XKB keymap parameter",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""},
    },
    SConfigOptionDescription{
        .value       = "input:kb_rules",
        .description = "Appropriate XKB keymap parameter",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""},
    },
    SConfigOptionDescription{
        .value       = "input:kb_file",
        .description = "Appropriate XKB keymap file",
        .type        = CONFIG_OPTION_STRING_LONG,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "input:numlock_by_default",
        .description = "Engage numlock by default.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:resolve_binds_by_sym",
        .description = "Determines how keybinds act when multiple layouts are used. If false, keybinds will always act as if the first specified layout is active. If true, "
                       "keybinds specified by symbols are activated when you type the respective symbol with the current layout.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:repeat_rate",
        .description = "The repeat rate for held-down keys, in repeats per second.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{25, 0, 200},
    },
    SConfigOptionDescription{
        .value       = "input:repeat_delay",
        .description = "Delay before a held-down key is repeated, in milliseconds.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{600, 0, 2000},
    },
    SConfigOptionDescription{
        .value       = "input:sensitivity",
        .description = "Sets the mouse input sensitivity. Value is clamped to the range -1.0 to 1.0.",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0, -1, 1},
    },
    SConfigOptionDescription{
        .value       = "input:accel_profile",
        .description = "Sets the cursor acceleration profile. Can be one of adaptive, flat. Can also be custom, see below. Leave empty to use libinput's default mode for your "
                       "input device. [adaptive/flat/custom]",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "input:force_no_accel",
        .description = "Force no cursor acceleration. This bypasses most of your pointer settings to get as raw of a signal as possible. Enabling this is not recommended due to "
                       "potential cursor desynchronization.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:left_handed",
        .description = "Switches RMB and LMB",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:scroll_points",
        .description = "Sets the scroll acceleration profile, when accel_profile is set to custom. Has to be in the form <step> <points>. Leave empty to have a flat scroll curve.",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "input:scroll_method",
        .description = "Sets the scroll method. Can be one of 2fg (2 fingers), edge, on_button_down, no_scroll. [2fg/edge/on_button_down/no_scroll]",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "input:scroll_button",
        .description = "Sets the scroll button. Has to be an int, cannot be a string. Check wev if you have any doubts regarding the ID. 0 means default.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 300},
    },
    SConfigOptionDescription{
        .value       = "input:scroll_button_lock",
        .description = "If the scroll button lock is enabled, the button does not need to be held down. Pressing and releasing the button toggles the button lock, which logically "
                       "holds the button down or releases it. While the button is logically held down, motion events are converted to scroll events.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:scroll_factor",
        .description = "Multiplier added to scroll movement for external mice. Note that there is a separate setting for touchpad scroll_factor.",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "input:natural_scroll",
        .description = "Inverts scrolling direction. When enabled, scrolling moves content directly, rather than manipulating a scrollbar.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:follow_mouse",
        .description = "Specify if and how cursor movement should affect window focus. See the note below. [0/1/2/3]",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 3},
    },
    SConfigOptionDescription{
        .value       = "input:follow_mouse_threshold",
        .description = "The smallest distance in logical pixels the mouse needs to travel for the window under it to get focused. Works only with follow_mouse = 1.",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{},
    },
    SConfigOptionDescription{
        .value       = "input:focus_on_close",
        .description = "Controls the window focus behavior when a window is closed. When set to 0, focus will shift to the next window candidate. When set to 1, focus will shift "
                       "to the window under the cursor.",
        .type        = CONFIG_OPTION_CHOICE,
        .data        = SConfigOptionDescription::SChoiceData{0, "next,cursor"},
    },
    SConfigOptionDescription{
        .value       = "input:mouse_refocus",
        .description = "if disabled, mouse focus won't switch to the hovered window unless the mouse crosses a window boundary when follow_mouse=1.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "input:float_switch_override_focus",
        .description = "If enabled (1 or 2), focus will change to the window under the cursor when changing from tiled-to-floating and vice versa. If 2, focus will also follow "
                       "mouse on float-to-float switches.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "input:special_fallthrough",
        .description = "if enabled, having only floating windows in the special workspace will not block focusing windows in the regular workspace.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:off_window_axis_events",
        .description = "Handles axis events around (gaps/border for tiled, dragarea/border for floated) a focused window. 0 ignores axis events 1 sends out-of-bound coordinates 2 "
                       "fakes pointer coordinates to the closest point inside the window 3 warps the cursor to the closest point inside the window",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 3},
    },
    SConfigOptionDescription{
        .value       = "input:emulate_discrete_scroll",
        .description = "Emulates discrete scrolling from high resolution scrolling events. 0 disables it, 1 enables handling of non-standard events only, and 2 force enables all "
                       "scroll wheel events to be handled",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 2},
    },

    /*
     * input:touchpad:
     */

    SConfigOptionDescription{
        .value       = "input:touchpad:disable_while_typing",
        .description = "Disable the touchpad while typing.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:natural_scroll",
        .description = "Inverts scrolling direction. When enabled, scrolling moves content directly, rather than manipulating a scrollbar.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:scroll_factor",
        .description = "Multiplier applied to the amount of scroll movement.",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0, 2},
    },
    SConfigOptionDescription{
        .value = "input:touchpad:middle_button_emulation",
        .description =
            "Sending LMB and RMB simultaneously will be interpreted as a middle click. This disables any touchpad area that would normally send a middle click based on location.",
        .type = CONFIG_OPTION_BOOL,
        .data = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:tap_button_map",
        .description = "Sets the tap button mapping for touchpad button emulation. Can be one of lrm (default) or lmr (Left, Middle, Right Buttons). [lrm/lmr]",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value = "input:touchpad:clickfinger_behavior",
        .description =
            "Button presses with 1, 2, or 3 fingers will be mapped to LMB, RMB, and MMB respectively. This disables interpretation of clicks based on location on the touchpad.",
        .type = CONFIG_OPTION_BOOL,
        .data = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:tap-to-click",
        .description = "Tapping on the touchpad with 1, 2, or 3 fingers will send LMB, RMB, and MMB respectively.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:drag_lock",
        .description = "When enabled, lifting the finger off while dragging will not drop the dragged item. 0 -> disabled, 1 -> enabled with timeout, 2 -> enabled sticky."
                       "dragging will not drop the dragged item.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:tap-and-drag",
        .description = "Sets the tap and drag mode for the touchpad",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:flip_x",
        .description = "Inverts the horizontal movement of the touchpad",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:flip_y",
        .description = "Inverts the vertical movement of the touchpad",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:touchpad:drag_3fg",
        .description = "Three Finger Drag 0 -> disabled, 1 -> 3 finger, 2 -> 4 finger",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 2},
    },

    /*
     * input:touchdevice:
     */

    SConfigOptionDescription{
        .value       = "input:touchdevice:transform",
        .description = "Transform the input from touchdevices. The possible transformations are the same as those of the monitors",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 6}, // ##TODO RANGE?
    },
    SConfigOptionDescription{
        .value       = "input:touchdevice:output",
        .description = "The monitor to bind touch devices. The default is auto-detection. To stop auto-detection, use an empty string or the [[Empty]] value.",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "input:touchdevice:enabled",
        .description = "Whether input is enabled for touch devices.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },

    /*
     * input:virtualkeyboard:
     */

    SConfigOptionDescription{
        .value       = "input:virtualkeyboard:share_states",
        .description = "Unify key down states and modifier states with other keyboards. 0 -> no, 1 -> yes, 2 -> yes unless IME client",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{2, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "input:virtualkeyboard:release_pressed_on_close",
        .description = "Release all pressed keys by virtual keyboard on close.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * input:tablet:
     */

    SConfigOptionDescription{
        .value       = "input:tablet:transform",
        .description = "transform the input from tablets. The possible transformations are the same as those of the monitors",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 6}, // ##TODO RANGE?
    },
    SConfigOptionDescription{
        .value       = "input:tablet:output",
        .description = "the monitor to bind tablets. Can be current or a monitor name. Leave empty to map across all monitors.",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "input:tablet:region_position",
        .description = "position of the mapped region in monitor layout relative to the top left corner of the bound monitor or all monitors.",
        .type        = CONFIG_OPTION_VECTOR,
        .data        = SConfigOptionDescription::SVectorData{{}, {-20000, -20000}, {20000, 20000}},
    },
    SConfigOptionDescription{
        .value       = "input:tablet:absolute_region_position",
        .description = "whether to treat the region_position as an absolute position in monitor layout. Only applies when output is empty.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:tablet:region_size",
        .description = "size of the mapped region. When this variable is set, tablet input will be mapped to the region. [0, 0] or invalid size means unset.",
        .type        = CONFIG_OPTION_VECTOR,
        .data        = SConfigOptionDescription::SVectorData{{}, {-100, -100}, {4000, 4000}},
    },
    SConfigOptionDescription{
        .value       = "input:tablet:relative_input",
        .description = "whether the input should be relative",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:tablet:left_handed",
        .description = "if enabled, the tablet will be rotated 180 degrees",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "input:tablet:active_area_size",
        .description = "size of tablet's active area in mm",
        .type        = CONFIG_OPTION_VECTOR,
        .data        = SConfigOptionDescription::SVectorData{{}, {}, {500, 500}},
    },
    SConfigOptionDescription{
        .value       = "input:tablet:active_area_position",
        .description = "position of the active area in mm",
        .type        = CONFIG_OPTION_VECTOR,
        .data        = SConfigOptionDescription::SVectorData{{}, {}, {500, 500}},
    },

    /* ##TODO
     *
     * PER DEVICE SETTINGS?
     *
     * */

    /*
     * gestures:
     */

    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_distance",
        .description = "in px, the distance of the touchpad gesture",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{300, 0, 2000}, //##TODO RANGE?
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_touch",
        .description = "enable workspace swiping from the edge of a touchscreen",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_invert",
        .description = "invert the direction (touchpad only)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_touch_invert",
        .description = "invert the direction (touchscreen only)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_min_speed_to_force",
        .description = "minimum speed in px per timepoint to force the change ignoring cancel_ratio. Setting to 0 will disable this mechanic.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{30, 0, 200}, //##TODO RANGE?
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_cancel_ratio",
        .description = "how much the swipe has to proceed in order to commence it. (0.7 -> if > 0.7 * distance, switch, if less, revert) [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.5, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_create_new",
        .description = "whether a swipe right on the last workspace should create a new one.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_direction_lock",
        .description = "if enabled, switching direction will be locked when you swipe past the direction_lock_threshold (touchpad only).",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_direction_lock_threshold",
        .description = "in px, the distance to swipe before direction lock activates (touchpad only).",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{10, 0, 200}, //##TODO RANGE?
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_forever",
        .description = "if enabled, swiping will not clamp at the neighboring workspaces but continue to the further ones.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "gestures:workspace_swipe_use_r",
        .description = "if enabled, swiping will use the r prefix instead of the m prefix for finding workspaces.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "gestures:close_max_timeout",
        .description = "Timeout for closing windows with the close gesture, in ms.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1000, 10, 2000},
    },

    /*
     * group:
     */

    SConfigOptionDescription{
        .value       = "group:insert_after_current",
        .description = "whether new windows in a group spawn after current or at group tail",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:focus_removed_window",
        .description = "whether Hyprland should focus on the window that has just been moved out of the group",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:merge_groups_on_drag",
        .description = "whether window groups can be dragged into other groups",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:merge_groups_on_groupbar",
        .description = "whether one group will be merged with another when dragged into its groupbar",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:col.border_active",
        .description = "border color for inactive windows",
        .type        = CONFIG_OPTION_GRADIENT,
        .data        = SConfigOptionDescription::SGradientData{"0x66ffff00"},
    },
    SConfigOptionDescription{
        .value       = "group:col.border_inactive",
        .description = "border color for the active window",
        .type        = CONFIG_OPTION_GRADIENT,
        .data        = SConfigOptionDescription::SGradientData{"0x66777700"},
    },
    SConfigOptionDescription{
        .value       = "group:col.border_locked_inactive",
        .description = "inactive border color for window that cannot be added to a group (see denywindowfromgroup dispatcher)",
        .type        = CONFIG_OPTION_GRADIENT,
        .data        = SConfigOptionDescription::SGradientData{"0x66ff5500"},
    },
    SConfigOptionDescription{
        .value       = "group:col.border_locked_active",
        .description = "active border color for window that cannot be added to a group",
        .type        = CONFIG_OPTION_GRADIENT,
        .data        = SConfigOptionDescription::SGradientData{"0x66775500"},
    },
    SConfigOptionDescription{
        .value       = "group:auto_group",
        .description = "automatically group new windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:drag_into_group",
        .description = "whether dragging a window into a unlocked group will merge them. Options: 0 (disabled), 1 (enabled), 2 (only when dragging into the groupbar)",
        .type        = CONFIG_OPTION_CHOICE,
        .data        = SConfigOptionDescription::SChoiceData{0, "disabled,enabled,only when dragging into the groupbar"},
    },
    SConfigOptionDescription{
        .value       = "group:merge_floated_into_tiled_on_groupbar",
        .description = "whether dragging a floating window into a tiled window groupbar will merge them",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "group:group_on_movetoworkspace",
        .description = "whether using movetoworkspace[silent] will merge the window into the workspace's solitary unlocked group",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * group:groupbar:
     */

    SConfigOptionDescription{
        .value       = "group:groupbar:enabled",
        .description = "enables groupbars",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:font_family",
        .description = "font used to display groupbar titles, use misc:font_family if not specified",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{STRVAL_EMPTY}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:font_weight_active",
        .description = "weight of the font used to display active groupbar titles",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"normal"},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:font_weight_inactive",
        .description = "weight of the font used to display inactive groupbar titles",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"normal"},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:font_size",
        .description = "font size of groupbar title",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{8, 2, 64},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:gradients",
        .description = "enables gradients",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:height",
        .description = "height of the groupbar",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{14, 1, 64},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:indicator_gap",
        .description = "height of the gap between the groupbar indicator and title",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 64},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:indicator_height",
        .description = "height of the groupbar indicator",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{3, 1, 64},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:stacked",
        .description = "render the groupbar as a vertical stack",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:priority",
        .description = "sets the decoration priority for groupbars",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{3, 0, 6}, //##TODO RANGE?
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:render_titles",
        .description = "whether to render titles in the group bar decoration",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:scrolling",
        .description = "whether scrolling in the groupbar changes group active window",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:rounding",
        .description = "how much to round the groupbar",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:rounding_power",
        .description = "rounding power of groupbar corners (2 is a circle)",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{2, 2, 10},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:gradient_rounding",
        .description = "how much to round the groupbar gradient",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:gradient_rounding_power",
        .description = "rounding power of groupbar gradient corners (2 is a circle)",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{2, 2, 10},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:round_only_edges",
        .description = "if yes, will only round at the groupbar edges",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:gradient_round_only_edges",
        .description = "if yes, will only round at the groupbar gradient edges",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:text_color",
        .description = "color for window titles in the groupbar",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{0xffffffff},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:text_color_inactive",
        .description = "color for inactive windows' titles in the groupbar (if unset, defaults to text_color)",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{}, //TODO: UNSET?
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:text_color_locked_active",
        .description = "color for the active window's title in a locked group (if unset, defaults to text_color)",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{}, //TODO: UNSET?
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:text_color_locked_inactive",
        .description = "color for inactive windows' titles in locked groups (if unset, defaults to text_color_inactive)",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{}, //TODO: UNSET?
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:col.active",
        .description = "active group border color",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{0x66ffff00},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:col.inactive",
        .description = "inactive (out of focus) group border color",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{0x66777700},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:col.locked_active",
        .description = "active locked group border color",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{0x66ff5500},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:col.locked_inactive",
        .description = "controls the group bar text color",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{0x66775500},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:gaps_out",
        .description = "gap between gradients and window",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{2, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:gaps_in",
        .description = "gap between gradients",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{2, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:keep_upper_gap",
        .description = "keep an upper gap above gradient",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "group:groupbar:text_offset",
        .description = "set an offset for a text",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SRangeData{0, -20, 20},
    },

    /*
     * misc:
     */

    SConfigOptionDescription{
        .value       = "misc:disable_hyprland_logo",
        .description = "disables the random Hyprland logo / anime girl background. :(",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:disable_splash_rendering",
        .description = "disables the Hyprland splash rendering. (requires a monitor reload to take effect)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:col.splash",
        .description = "Changes the color of the splash text (requires a monitor reload to take effect).",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{0xffffffff},
    },
    SConfigOptionDescription{
        .value       = "misc:font_family",
        .description = "Set the global default font to render the text including debug fps/notification, config error messages and etc., selected from system fonts.",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"Sans"},
    },
    SConfigOptionDescription{
        .value       = "misc:splash_font_family",
        .description = "Changes the font used to render the splash text, selected from system fonts (requires a monitor reload to take effect).",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{STRVAL_EMPTY}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "misc:force_default_wallpaper",
        .description = "Enforce any of the 3 default wallpapers. Setting this to 0 or 1 disables the anime background. -1 means “random”. [-1/0/1/2]",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{-1, -1, 2},
    },
    SConfigOptionDescription{
        .value       = "misc:vfr",
        .description = "controls the VFR status of Hyprland. Heavily recommended to leave enabled to conserve resources.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:vrr",
        .description = "	controls the VRR (Adaptive Sync) of your monitors. 0 - off, 1 - on, 2 - fullscreen only, 3 - fullscreen with game or video content type [0/1/2/3]",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{.value = 0, .min = 0, .max = 3},
    },
    SConfigOptionDescription{
        .value       = "misc:mouse_move_enables_dpms",
        .description = "If DPMS is set to off, wake up the monitors if the mouse move",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:key_press_enables_dpms",
        .description = "If DPMS is set to off, wake up the monitors if a key is pressed.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:name_vk_after_proc",
        .description = "Name virtual keyboards after the processes that create them. E.g. /usr/bin/fcitx5 will have hl-virtual-keyboard-fcitx5.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:always_follow_on_dnd",
        .description = "Will make mouse focus follow the mouse when drag and dropping. Recommended to leave it enabled, especially for people using focus follows mouse at 0.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:layers_hog_keyboard_focus",
        .description = "If true, will make keyboard-interactive layers keep their focus on mouse move (e.g. wofi, bemenu)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:animate_manual_resizes",
        .description = "If true, will animate manual window resizes/moves",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:animate_mouse_windowdragging",
        .description = "If true, will animate windows being dragged by mouse, note that this can cause weird behavior on some curves",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:disable_autoreload",
        .description = "If true, the config will not reload automatically on save, and instead needs to be reloaded with hyprctl reload. Might save on battery.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:enable_swallow",
        .description = "Enable window swallowing",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value = "misc:swallow_regex",
        .description =
            "The class regex to be used for windows that should be swallowed (usually, a terminal). To know more about the list of regex which can be used use this cheatsheet.",
        .type = CONFIG_OPTION_STRING_SHORT,
        .data = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "misc:swallow_exception_regex",
        .description = "The title regex to be used for windows that should not be swallowed by the windows specified in swallow_regex (e.g. wev). The regex is matched against the "
                       "parent (e.g. Kitty) window’s title on the assumption that it changes to whatever process it’s running.",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "misc:focus_on_activate",
        .description = "Whether Hyprland should focus an app that requests to be focused (an activate request)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:mouse_move_focuses_monitor",
        .description = "Whether mouse moving into a different monitor should focus it",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:allow_session_lock_restore",
        .description = "if true, will allow you to restart a lockscreen app in case it crashes (red screen of death)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:session_lock_xray",
        .description = "keep rendering workspaces below your lockscreen",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:background_color",
        .description = "change the background color. (requires enabled disable_hyprland_logo)",
        .type        = CONFIG_OPTION_COLOR,
        .data        = SConfigOptionDescription::SColorData{0x111111},
    },
    SConfigOptionDescription{
        .value       = "misc:close_special_on_empty",
        .description = "close the special workspace if the last window is removed",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:new_window_takes_over_fullscreen",
        .description = "if there is a fullscreen or maximized window, decide whether a new tiled window opened should replace it, stay behind or disable the fullscreen/maximized "
                       "state. 0 - behind, 1 - takes over, 2 - unfullscreen/unmaxize [0/1/2]",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "misc:exit_window_retains_fullscreen",
        .description = "if true, closing a fullscreen window makes the next focused window fullscreen",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:initial_workspace_tracking",
        .description = "if enabled, windows will open on the workspace they were invoked on. 0 - disabled, 1 - single-shot, 2 - persistent (all children too)",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "misc:middle_click_paste",
        .description = "whether to enable middle-click-paste (aka primary selection)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:render_unfocused_fps",
        .description = "the maximum limit for renderunfocused windows' fps in the background",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{15, 1, 120},
    },
    SConfigOptionDescription{
        .value       = "misc:disable_xdg_env_checks",
        .description = "disable the warning if XDG environment is externally managed",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:disable_hyprland_qtutils_check",
        .description = "disable the warning if hyprland-qtutils is missing",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "misc:lockdead_screen_delay",
        .description = "the delay in ms after the lockdead screen appears if the lock screen did not appear after a lock event occurred.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1000, 0, 5000},
    },
    SConfigOptionDescription{
        .value       = "misc:enable_anr_dialog",
        .description = "whether to enable the ANR (app not responding) dialog when your apps hang",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:anr_missed_pings",
        .description = "number of missed pings before showing the ANR dialog",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 1, 10},
    },
    SConfigOptionDescription{
        .value       = "misc:screencopy_force_8b",
        .description = "forces 8 bit screencopy (fixes apps that don't understand 10bit)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "misc:disable_scale_notification",
        .description = "disables notification popup when a monitor fails to set a suitable scale and falls back to suggested",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * binds:
     */

    SConfigOptionDescription{
        .value       = "binds:pass_mouse_when_bound",
        .description = "if disabled, will not pass the mouse events to apps / dragging windows around if a keybind has been triggered.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:scroll_event_delay",
        .description = "in ms, how many ms to wait after a scroll event to allow passing another one for the binds.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{300, 0, 2000},
    },
    SConfigOptionDescription{
        .value       = "binds:workspace_back_and_forth",
        .description = "If enabled, an attempt to switch to the currently focused workspace will instead switch to the previous workspace. Akin to i3’s auto_back_and_forth.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:hide_special_on_workspace_change",
        .description = "If enabled, changing the active workspace (including to itself) will hide the special workspace on the monitor where the newly active workspace resides.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:allow_workspace_cycles",
        .description = "If enabled, workspaces don’t forget their previous workspace, so cycles can be created by switching to the first workspace in a sequence, then endlessly "
                       "going to the previous workspace.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:workspace_center_on",
        .description = "Whether switching workspaces should center the cursor on the workspace (0) or on the last active window for that workspace (1)",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "binds:focus_preferred_method",
        .description = "sets the preferred focus finding method when using focuswindow/movewindow/etc with a direction. 0 - history (recent have priority), 1 - length (longer "
                       "shared edges have priority)",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "binds:ignore_group_lock",
        .description = "If enabled, dispatchers like moveintogroup, moveoutofgroup and movewindoworgroup will ignore lock per group.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:movefocus_cycles_fullscreen",
        .description = "If enabled, when on a fullscreen window, movefocus will cycle fullscreen, if not, it will move the focus in a direction.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:movefocus_cycles_groupfirst",
        .description = "If enabled, when in a grouped window, movefocus will cycle windows in the groups first, then at each ends of tabs, it'll move on to other windows/groups",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:disable_keybind_grabbing",
        .description = "If enabled, apps that request keybinds to be disabled (e.g. VMs) will not be able to do so.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:window_direction_monitor_fallback",
        .description = "If enabled, moving a window or focus over the edge of a monitor with a direction will move it to the next monitor in that direction.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "binds:allow_pin_fullscreen",
        .description = "Allows fullscreen to pinned windows, and restore their pinned status afterwards",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "binds:drag_threshold",
        .description = "Movement threshold in pixels for window dragging and c/g bind flags. 0 to disable and grab on mousedown.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, INT_MAX},
    },

    /*
     * xwayland:
     */

    SConfigOptionDescription{
        .value       = "xwayland:enabled",
        .description = "allow running applications using X11",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "xwayland:use_nearest_neighbor",
        .description = "uses the nearest neighbor filtering for xwayland apps, making them pixelated rather than blurry",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "xwayland:force_zero_scaling",
        .description = "forces a scale of 1 on xwayland windows on scaled displays.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "xwayland:create_abstract_socket",
        .description = "Create the abstract Unix domain socket for XWayland",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * opengl:
     */

    SConfigOptionDescription{
        .value       = "opengl:nvidia_anti_flicker",
        .description = "reduces flickering on nvidia at the cost of possible frame drops on lower-end GPUs. On non-nvidia, this is ignored.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },

    /*
     * render:
     */

    SConfigOptionDescription{
        .value       = "render:direct_scanout",
        .description = "Enables direct scanout. Direct scanout attempts to reduce lag when there is only one fullscreen application on a screen (e.g. game). It is also "
                       "recommended to set this to false if the fullscreen application shows graphical glitches. 0 - off, 1 - on, 2 - auto (on with content type 'game')",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{.value = 0, .min = 0, .max = 2},
    },
    SConfigOptionDescription{
        .value       = "render:expand_undersized_textures",
        .description = "Whether to expand textures that have not yet resized to be larger, or to just stretch them instead.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "render:xp_mode",
        .description = "Disable back buffer and bottom layer rendering.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "render:ctm_animation",
        .description = "Whether to enable a fade animation for CTM changes (hyprsunset). 2 means 'auto' (Yes on everything but Nvidia).",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{2, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "render:cm_fs_passthrough",
        .description = "Passthrough color settings for fullscreen apps when possible",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{.value = 2, .min = 0, .max = 2},
    },
    SConfigOptionDescription{
        .value       = "render:cm_enabled",
        .description = "Enable Color Management pipelines (requires restart to fully take effect)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "render:send_content_type",
        .description = "Report content type to allow monitor profile autoswitch (may result in a black screen during the switch)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "render:cm_auto_hdr",
        .description = "Auto-switch to hdr mode when fullscreen app is in hdr, 0 - off, 1 - hdr, 2 - hdredid (cm_fs_passthrough can switch to hdr even when this setting is off)",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{.value = 1, .min = 0, .max = 2},
    },
    SConfigOptionDescription{
        .value       = "render:new_render_scheduling",
        .description = "enable new render scheduling, which should improve FPS on underpowered devices. This does not add latency when your PC can keep up.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * cursor:
     */

    SConfigOptionDescription{
        .value       = "cursor:invisible",
        .description = "don't render cursors",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "cursor:no_hardware_cursors",
        .description = "disables hardware cursors. Auto = disable when multi-gpu on nvidia",
        .type        = CONFIG_OPTION_CHOICE,
        .data        = SConfigOptionDescription::SChoiceData{0, "Disabled,Enabled,Auto"},
    },
    SConfigOptionDescription{
        .value       = "cursor:no_break_fs_vrr",
        .description = "disables scheduling new frames on cursor movement for fullscreen apps with VRR enabled to avoid framerate spikes (may require no_hardware_cursors = true) "
                       "0 - off, 1 - on, 2 - auto (on with content type 'game')",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{.value = 2, .min = 0, .max = 2},
    },
    SConfigOptionDescription{
        .value       = "cursor:min_refresh_rate",
        .description = "minimum refresh rate for cursor movement when no_break_fs_vrr is active. Set to minimum supported refresh rate or higher",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{24, 10, 500},
    },
    SConfigOptionDescription{
        .value       = "cursor:hotspot_padding",
        .description = "the padding, in logical px, between screen edges and the cursor",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "cursor:inactive_timeout",
        .description = "in seconds, after how many seconds of cursor’s inactivity to hide it. Set to 0 for never.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "cursor:no_warps",
        .description = "if true, will not warp the cursor in many cases (focusing, keybinds, etc)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "cursor:persistent_warps",
        .description = "When a window is refocused, the cursor returns to its last position relative to that window, rather than to the centre.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "cursor:warp_on_change_workspace",
        .description = "Move the cursor to the last focused window after changing the workspace. Options: 0 (Disabled), 1 (Enabled), 2 (Force - ignores cursor:no_warps option)",
        .type        = CONFIG_OPTION_CHOICE,
        .data        = SConfigOptionDescription::SChoiceData{0, "Disabled,Enabled,Force"},
    },
    SConfigOptionDescription{
        .value       = "cursor:warp_on_toggle_special",
        .description = "Move the cursor to the last focused window when toggling a special workspace. Options: 0 (Disabled), 1 (Enabled), "
                       "2 (Force - ignores cursor:no_warps option)",
        .type        = CONFIG_OPTION_CHOICE,
        .data        = SConfigOptionDescription::SChoiceData{0, "Disabled,Enabled,Force"},
    },
    SConfigOptionDescription{
        .value       = "cursor:default_monitor",
        .description = "the name of a default monitor for the cursor to be set to on startup (see hyprctl monitors for names)",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{""}, //##TODO UNSET?
    },
    SConfigOptionDescription{
        .value       = "cursor:zoom_factor",
        .description = "the factor to zoom by around the cursor. Like a magnifying glass. Minimum 1.0 (meaning no zoom)",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 1, 10},
    },
    SConfigOptionDescription{
        .value       = "cursor:zoom_rigid",
        .description = "whether the zoom should follow the cursor rigidly (cursor is always centered if it can be) or loosely",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "cursor:enable_hyprcursor",
        .description = "whether to enable hyprcursor support",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "cursor:hide_on_key_press",
        .description = "Hides the cursor when you press any key until the mouse is moved.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "cursor:hide_on_touch",
        .description = "Hides the cursor when the last input was a touch input until a mouse input is done.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "cursor:use_cpu_buffer",
        .description = "Makes HW cursors use a CPU buffer. Required on Nvidia to have HW cursors. Experimental",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "cursor:sync_gsettings_theme",
        .description = "sync xcursor theme with gsettings, it applies cursor-theme and cursor-size on theme load to gsettings making most CSD gtk based clients use same xcursor "
                       "theme and size.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "cursor:warp_back_after_non_mouse_input",
        .description = "warp the cursor back to where it was after using a non-mouse input to move it, and then returning back to mouse.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * ecosystem:
     */
    SConfigOptionDescription{
        .value       = "ecosystem:no_update_news",
        .description = "disable the popup that shows up when you update hyprland to a new version.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "ecosystem:no_donation_nag",
        .description = "disable the popup that shows up twice a year encouraging to donate.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "ecosystem:enforce_permissions",
        .description = "whether to enable permission control (see https://wiki.hypr.land/Configuring/Permissions/).",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * debug:
     */

    SConfigOptionDescription{
        .value       = "debug:overlay",
        .description = "print the debug performance overlay. Disable VFR for accurate results.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "debug:damage_blink",
        .description = "disable logging to a file",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "debug:disable_logs",
        .description = "disable logging to a file",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "debug:disable_time",
        .description = "disables time logging",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "debug:damage_tracking",
        .description = "redraw only the needed bits of the display. Do not change. (default: full - 2) monitor - 1, none - 0",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{2, 0, 2},
    },
    SConfigOptionDescription{
        .value       = "debug:enable_stdout_logs",
        .description = "enables logging to stdout",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "debug:manual_crash",
        .description = "set to 1 and then back to 0 to crash Hyprland.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "debug:suppress_errors",
        .description = "if true, do not display config file parsing errors.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "debug:disable_scale_checks",
        .description = "disables verification of the scale factors. Will result in pixel alignment and rounding errors.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "debug:error_limit",
        .description = "limits the number of displayed config file parsing errors.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{5, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "debug:error_position",
        .description = "sets the position of the error bar. top - 0, bottom - 1",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "debug:colored_stdout_logs",
        .description = "enables colors in the stdout logs.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "debug:log_damage",
        .description = "enables logging the damage.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "debug:pass",
        .description = "enables render pass debugging.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "debug:full_cm_proto",
        .description = "claims support for all cm proto features (requires restart)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * dwindle:
     */

    SConfigOptionDescription{
        .value       = "dwindle:pseudotile",
        .description = "enable pseudotiling. Pseudotiled windows retain their floating size when tiled.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "dwindle:force_split",
        .description = "0 -> split follows mouse, 1 -> always split to the left (new = left or top) 2 -> always split to the right (new = right or bottom)",
        .type        = CONFIG_OPTION_CHOICE,
        .data        = SConfigOptionDescription::SChoiceData{0, "follow mouse,left or top,right or bottom"},
    },
    SConfigOptionDescription{
        .value       = "dwindle:preserve_split",
        .description = "if enabled, the split (side/top) will not change regardless of what happens to the container.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "dwindle:smart_split",
        .description = "if enabled, allows a more precise control over the window split direction based on the cursor's position. The window is conceptually divided into four "
                       "triangles, and cursor's triangle determines the split direction. This feature also turns on preserve_split.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value = "dwindle:smart_resizing",
        .description =
            "if enabled, resizing direction will be determined by the mouse's position on the window (nearest to which corner). Else, it is based on the window's tiling position.",
        .type = CONFIG_OPTION_BOOL,
        .data = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "dwindle:permanent_direction_override",
        .description = "if enabled, makes the preselect direction persist until either this mode is turned off, another direction is specified, or a non-direction is specified "
                       "(anything other than l,r,u/t,d/b)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "dwindle:special_scale_factor",
        .description = "specifies the scale factor of windows on the special workspace [0 - 1]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "dwindle:split_width_multiplier",
        .description = "specifies the auto-split width multiplier",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0.1, 3},
    },
    SConfigOptionDescription{
        .value       = "dwindle:use_active_for_splits",
        .description = "whether to prefer the active window or the mouse position for splits",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "dwindle:default_split_ratio",
        .description = "the default split ratio on window open. 1 means even 50/50 split. [0.1 - 1.9]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0.1, 1.9},
    },
    SConfigOptionDescription{
        .value       = "dwindle:split_bias",
        .description = "specifies which window will receive the split ratio. 0 -> directional (the top or left window), 1 -> the current window",
        .type        = CONFIG_OPTION_CHOICE,
        .data        = SConfigOptionDescription::SChoiceData{0, "directional,current"},
    },
    SConfigOptionDescription{
        .value       = "dwindle:precise_mouse_move",
        .description = "if enabled, bindm movewindow will drop the window more precisely depending on where your mouse is.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "dwindle:single_window_aspect_ratio",
        .description = "If specified, whenever only a single window is open, it will be coerced into the specified aspect ratio.  Ignored if the y-value is zero.",
        .type        = CONFIG_OPTION_VECTOR,
        .data        = SConfigOptionDescription::SVectorData{{0, 0}, {0, 0}, {1000., 1000.}},
    },
    SConfigOptionDescription{
        .value       = "dwindle:single_window_aspect_ratio_tolerance",
        .description = "Minimum distance for single_window_aspect_ratio to take effect, in fractions of the monitor's size.",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{0.1f, 0.f, 1.f},
    },

    /*
     * master:
     */

    SConfigOptionDescription{
        .value       = "master:allow_small_split",
        .description = "enable adding additional master windows in a horizontal split style",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "master:special_scale_factor",
        .description = "the scale of the special workspace windows. [0.0 - 1.0]",
        .type        = CONFIG_OPTION_FLOAT,
        .data        = SConfigOptionDescription::SFloatData{1, 0, 1},
    },
    SConfigOptionDescription{
        .value = "master:mfact",
        .description =
            "the size as a percentage of the master window, for example `mfact = 0.70` would mean 70% of the screen will be the master window, and 30% the slave [0.0 - 1.0]",
        .type = CONFIG_OPTION_FLOAT,
        .data = SConfigOptionDescription::SFloatData{0.55, 0, 1},
    },
    SConfigOptionDescription{
        .value       = "master:new_status",
        .description = "`master`: new window becomes master; `slave`: new windows are added to slave stack; `inherit`: inherit from focused window",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"slave"},
    },
    SConfigOptionDescription{
        .value       = "master:new_on_top",
        .description = "whether a newly open window should be on the top of the stack",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "master:new_on_active",
        .description = "`before`, `after`: place new window relative to the focused window; `none`: place new window according to the value of `new_on_top`. ",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"none"},
    },
    SConfigOptionDescription{
        .value       = "master:orientation",
        .description = "default placement of the master area, can be left, right, top, bottom or center",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"left"},
    },
    SConfigOptionDescription{
        .value       = "master:inherit_fullscreen",
        .description = "inherit fullscreen status when cycling/swapping to another window (e.g. monocle layout)",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "master:slave_count_for_center_master",
        .description = "when using orientation=center, make the master window centered only when at least this many slave windows are open. (Set 0 to always_center_master)",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{2, 0, 10}, //##TODO RANGE?
    },
    SConfigOptionDescription{.value       = "master:center_master_fallback",
                             .description = "Set fallback for center master when slaves are less than slave_count_for_center_master, can be left ,right ,top ,bottom",
                             .type        = CONFIG_OPTION_STRING_SHORT,
                             .data        = SConfigOptionDescription::SStringData{"left"}},
    SConfigOptionDescription{
        .value       = "master:center_ignores_reserved",
        .description = "centers the master window on monitor ignoring reserved areas",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value = "master:smart_resizing",
        .description =
            "if enabled, resizing direction will be determined by the mouse's position on the window (nearest to which corner). Else, it is based on the window's tiling position.",
        .type = CONFIG_OPTION_BOOL,
        .data = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "master:drop_at_cursor",
        .description = "when enabled, dragging and dropping windows will put them at the cursor position. Otherwise, when dropped at the stack side, they will go to the "
                       "top/bottom of the stack depending on new_on_top.",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{true},
    },
    SConfigOptionDescription{
        .value       = "master:always_keep_position",
        .description = "whether to keep the master window in its configured position when there are no slave windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },

    /*
     * Experimental
    */

    SConfigOptionDescription{
        .value       = "experimental:xx_color_management_v4",
        .description = "enable color management protocol",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
};
