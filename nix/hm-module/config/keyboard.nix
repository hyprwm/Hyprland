{
  lib,
  types,
}: {
  layout = lib.mkOption {
    type = types.singleLineStr;
    default = "us";
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  variant = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  model = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  options = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  rules = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  file = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
}
