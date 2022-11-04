{
  lib,
  types,
}: {
  sensitivity = lib.mkOption {
    type = types.float;
    default = 1.0;
    description = lib.mdDoc ''
      **DEPRECATED**

      The sensitivity of all cursor devices.
      Do not use if specified in the *input* section.
    '';
    # example = lib.literalExpression "";
  };
  border_size = lib.mkOption {
    type = types.ints.unsigned;
    default = 1;
    description = lib.mdDoc ''
      The thickness of the window border, in pixels.
    '';
    # example = lib.literalExpression "";
  };
  no_border_on_floating = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Disable borders for floating windows.
    '';
    # example = lib.literalExpression "";
  };
  gaps_inside = lib.mkOption {
    type = types.ints.unsigned;
    default = 5;
    description = lib.mdDoc ''
      Gap thickness between window edges, in pixels.

      *Renamed from: `gaps_in`*
    '';
    # example = lib.literalExpression "";
  };
  gaps_outside = lib.mkOption {
    type = types.ints.unsigned;
    default = 20;
    description = lib.mdDoc ''
      Padding on the perimeter of the monitor, in pixels.

      *Renamed from: `gaps_out`*
    '';
    # example = lib.literalExpression "";
  };
  inactive_border_color = lib.mkOption {
    type = types.singleLineStr;
    default = "0xFFFFFFFF";
    description = lib.mdDoc ''
      The color of the border on an inactive window, in `0xAARRGGBB` format.

      *Renamed from: `col.inactive_border`*
    '';
    # example = lib.literalExpression "";
  };
  active_border_color = lib.mkOption {
    type = types.singleLineStr;
    default = "0xFFFFFFFF";
    description = lib.mdDoc ''
      The color of the border on an active window, in `0xAARRGGBB` format.

      *Renamed from: `col.active_border`*
    '';
    # example = lib.literalExpression "";
  };
  cursor_inactive_timeout = lib.mkOption {
    type = types.ints.positive;
    default = 0;
    description = lib.mdDoc ''
      Duration of measured inactivity before hiding the cursor.

      Use `0` to disable.
    '';
    # example = lib.literalExpression "";
  };
  layout = lib.mkOption {
    type = types.enum ["dwindle" "master"];
    default = "dwindle";
    description = lib.mdDoc ''
      The layout algorithm to use.

      Available modes: `dwindle`, `master`
    '';
    # example = lib.literalExpression "";
  };
  no_cursor_warps = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Whether to enable jumping the cursor when keyboard focus switches
      windows or similar event.
    '';
    # example = lib.literalExpression "";
  };
  apply_sens_to_raw = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      **ADVANCED**

      Apply the value of `sensitivity` to raw mouse input.
    '';
    # example = lib.literalExpression "";
  };
}
