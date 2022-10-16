{
  lib,
  types,
}: {
  pseudotile = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  group_border_color = lib.mkOption {
    type = types.singleLineStr;
    default = "0x66777700";
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  group_border_active_color = lib.mkOption {
    type = types.singleLineStr;
    default = "0x66FFFF00";
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  force_split = lib.mkOption {
    type = types.enum [0 1 2];
    default = 0;
    description = lib.mdDoc '''';
    example = lib.literalExpression "2";
  };
  preserve_split = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "true";
  };
  special_scale_factor = lib.mkOption {
    type = types.float;
    default = 0.8;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  split_width_multiplier = lib.mkOption {
    type = types.float;
    default = 1.0;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  no_gaps_when_only = lib.mkOption {
    type = types.bool;
    default = true; # false
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  use_active_for_splits = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
}
