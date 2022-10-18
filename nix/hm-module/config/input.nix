# TODO touchdevice <https://wiki.hyprland.org/Configuring/Variables/#touchdevice>
{
  lib,
  types,
} @ args: {
  # TODO touchdevice <https://wiki.hyprland.org/Configuring/Variables/#touchdevice>

  follow_mouse = lib.mkOption {
    type = types.enum [0 "disabled" 1 "full" 2 "loose" 3 "full_loose"];
    # type = types.enum [0 1 2 3];
    apply = x:
      if x == "disabled"
      then 0
      else if x == "full"
      then 1
      else if x == "loose"
      then 2
      else if x == "full_loose"
      then 3
      else lib.warn ''hyprland: follow_mouse: use enum string values'' x;
    default = "full";
    description = lib.mdDoc ''
      | Int | Enum | Description |
      | --- | ---- | ----------- |
      | `0` | `"disabled"` | Do not move focus to hovered window. |
      | `1` | `"full"` | Move focus to hovered window. |
      | `2` | `"loose"` | Move mouse focus but not keyboard focus. |
      | `3` | `"full_loose"` | Detach keyboard and mouse focus and do not refocus on click. |
    '';
    example = lib.literalExpression ''
      "loose"
    '';
  };
  float_switch_override_focus = lib.mkOption {
    type = types.enum [0 "disabled" 1 "tiled_to_float" 2 "float_to_float"];
    # type = types.enum [0 1 2];
    apply = x:
      if x == "disabled"
      then 0
      else if x == "tiled_to_float"
      then 1
      else if x == "float_to_float"
      then 2
      else lib.warn ''hyprland: float_switch_override_focus: use enum string values'' x;
    default = "disabled";
    description = lib.mdDoc ''
      | Int | Enum | Description |
      | --- | ---- | ----------- |
      | `0` | `disabled` | Do not move focus when hovering a floating window. |
      | `1` | `tiled_to_float` | Move focus to floating window when hovered. |
      | `2` | `float_to_float` | When hover changes from a floating or tiled window to another floating window, move focus. |
    '';
    example = lib.literalExpression ''
      "float_to_float"
    '';
  };
  repeat_rate = lib.mkOption {
    type = types.ints.positive;
    default = 25;
    description = lib.mdDoc ''
      Delay between registering repeated key presses (usually when a key is held) in milliseconds.
    '';
    # example = lib.literalExpression '''';
  };
  repeat_delay = lib.mkOption {
    type = types.ints.positive;
    default = 600;
    description = lib.mdDoc ''
      Delay before considering a held key a repeating event.
    '';
    # example = lib.literalExpression '''';
  };
  natural_scroll = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Whether to reverse scrolling to "natural" direction.

      <https://wayland.freedesktop.org/libinput/doc/latest/configuration.html#scrolling>
    '';
    # example = lib.literalExpression '''';
  };
  numlock_by_default = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Lock number pad on start.
    '';
    # example = lib.literalExpression '''';
  };
  force_no_accel = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Force mouse acceleration to be disabled,
      bypassing pointer settings and getting nearly raw input.
    '';
    # example = lib.literalExpression '''';
  };
  sensitivity = lib.mkOption {
    type = types.float;
    default = 0.0;
    description = lib.mdDoc ''
      Sensitivity value to give to `libinput`. Clamped between `-1` and `1`.

      <https://wayland.freedesktop.org/libinput/doc/latest/configuration.html#pointer-acceleration>
    '';
    # example = lib.literalExpression '''';
  };
  left_handed = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Switch right and left mouse button actions.

      <https://wayland.freedesktop.org/libinput/doc/latest/configuration.html#left-handed-mode>
    '';
    # example = lib.literalExpression '''';
  };
  accel_profile = lib.mkOption {
    # TODO why is the default `[EMPTY]`?
    type = types.enum ["adaptive" "flat"];
    default = "adaptive";
    description = lib.mdDoc ''
      Acceleration profile for `libinput`.

      <https://wayland.freedesktop.org/libinput/doc/latest/configuration.html#pointer-acceleration>
    '';
    example = lib.literalExpression '''';
  };
  scroll_method = lib.mkOption {
    # TODO why is the default `[EMPTY]`?
    type = types.enum ["2fg" "edge" "on_button_down" "no_scroll"];
    # type = types.nullOr (types.enum ["2fg" "edge" "on_button_down" "no_scroll"]);
    default = "2fg";
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };

  keyboard = import ./keyboard.nix args;
  touchpad = import ./touchpad.nix args;
}
