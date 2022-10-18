{
  lib,
  types,
}: {
  disable_hyprland_logo = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  disable_splash_rendering = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  no_vfr = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  damage_entire_on_snapshot = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  mouse_move_enables_dpms = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  always_follow_on_dnd = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  layers_hog_keyboard_focus = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  animate_manual_resizes = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  disable_autoreload = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  enable_swallow = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  swallow_regex = lib.mkOption {
    type = types.nullOr (types.listOf types.singleLineStr);
    default = null;
    description = lib.mdDoc ''
      Window classes to match when checking if a newly created window should
      swallow the parent.

      Either `null` or a list of regular expressions matching class names.
      See the option's `apply` attribute:

      ```nix
        apply = lib.mapNullable (x: "^(''${lib.concatStringsSep "|" x})");
      ```
    '';
    example = lib.literalExpression ''["Alacritty" "dolphin" "Steam"]'';
    apply = lib.mapNullable (x: "^(${lib.concatStringsSep "|" x})");
  };
}
