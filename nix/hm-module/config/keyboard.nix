{
  lib,
  types,
}: { # TODO make this better
  layout = lib.mkOption {
    type = types.singleLineStr;
    default = "us";
    description = lib.mdDoc ''
      Equivalent XKB keymap parameter.
    '';
    # example = lib.literalExpression '''';
  };
  variant = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''
      Equivalent XKB keymap parameter.
    '';
    # example = lib.literalExpression '''';
  };
  model = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''
      Equivalent XKB keymap parameter.
    '';
    # example = lib.literalExpression '''';
  };
  options = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''
      Equivalent XKB keymap parameter.
    '';
    # example = lib.literalExpression '''';
  };
  rules = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''
      Equivalent XKB keymap parameter.
    '';
    # example = lib.literalExpression '''';
  };
  file = lib.mkOption {
    type = types.nullOr types.singleLineStr;
    default = null;
    description = lib.mdDoc ''
      Equivalent XKB keymap parameter.
    '';
    # example = lib.literalExpression '''';
  };
}
