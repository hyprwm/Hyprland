{
  lib,
  types,
}: {
  overlay = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  damage_blink = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  disable_logs = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  disable_time = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  damage_tracking = lib.mkOption {
    type = types.enum ["none" "monitor" "full"];
    default = "full";
    description = lib.mdDoc ''
      **ADVANCED**

      Makes the compositor redraw only the changed pixels of the display.
      Saves on resources by not redrawing when not needed.

      Available modes: `none`, `monitor`, `full`

      You don't need to know what different modes do, just always use full.
    '';
    # example = lib.literalExpression "";
    apply = x:
      if x == "none"
      then 0
      else if x == "monitor"
      then 1
      else if x == "full"
      then 2
      else assert (lib.assertMsg false "unreachable code"); null;
  };
}
