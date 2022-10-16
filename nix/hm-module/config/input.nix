# TODO touchdevice <https://wiki.hyprland.org/Configuring/Variables/#touchdevice>
{
  lib,
  types,
}: {
  # TODO touchdevice <https://wiki.hyprland.org/Configuring/Variables/#touchdevice>

  follow_mouse = lib.mkOption {
    type = types.enum [0 1 2 3];
    default = 1;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  float_switch_override_focus = lib.mkOption {
    type = types.enum [0 1 2];
    default = 1;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  repeat_rate = lib.mkOption {
    type = types.ints.positive;
    default = 25;
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
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  numlock_by_default = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  force_no_accel = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  sensitivity = lib.mkOption {
    type = types.float;
    default = 0.0;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  left_handed = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  accel_profile = lib.mkOption {
    # TODO why is the default `[EMPTY]`?
    type = types.enum ["adaptive" "flat"];
    default = "adaptive";
    description = lib.mdDoc ''description'';
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
}
