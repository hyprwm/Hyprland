{
  lib,
  types,
}: {
  rounding = lib.mkOption {
    type = types.ints.unsigned;
    default = 0;
    description = lib.mdDoc '''';
  };
  multisample_edges = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
  };
  active_opacity = lib.mkOption {
    type = types.float;
    default = 1.0;
    description = lib.mdDoc '''';
  };
  inactive_opacity = lib.mkOption {
    type = types.float;
    default = 1.0;
    description = lib.mdDoc '''';
  };
  fullscreen_opacity = lib.mkOption {
    type = types.float;
    default = 1.0;
    description = lib.mdDoc '''';
  };
  blur = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
  };
  blur_size = lib.mkOption {
    type = types.ints.positive;
    default = 8;
    description = lib.mdDoc '''';
  };
  blur_passes = lib.mkOption {
    type = types.ints.positive;
    default = 1;
    description = lib.mdDoc '''';
  };
  blur_ignore_opacity = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
  };
  blur_new_optimizations = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
  };
  drop_shadow = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
  };
  shadow_range = lib.mkOption {
    type = types.ints.unsigned;
    default = 4;
    description = lib.mdDoc '''';
  };
  shadow_render_power = lib.mkOption {
    type = types.ints.unsigned;
    default = 3;
    description = lib.mdDoc '''';
  };
  shadow_ignore_window = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
  };
  active_shadow_color = lib.mkOption {
    type = types.singleLineStr;
    # TODO colors
    default = "0xee1a1a1a";
    description = lib.mdDoc '''';
  };
  inactive_shadow_color = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc '''';
  };
  shadow_offset = lib.mkOption {
    # TODO tuple
    type = types.listOf types.int;
    default = [0 0];
    description = lib.mdDoc '''';
  };
  dim_inactive = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
  };
  dim_strength = lib.mkOption {
    type = types.float;
    default = 0.5;
    description = lib.mdDoc '''';
  };
}
