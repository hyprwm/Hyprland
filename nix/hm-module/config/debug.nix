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
}
