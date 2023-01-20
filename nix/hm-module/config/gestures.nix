{
  lib,
  types,
}: {
  workspace_swipe = {
    enable = lib.mkOption {
      type = types.bool;
      default = false;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
    fingers = lib.mkOption {
      type = types.ints.positive;
      default = 3;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
    distance = lib.mkOption {
      type = types.ints.unsigned;
      default = 300;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
    invert = lib.mkOption {
      type = types.bool;
      default = true;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
    min_speed_to_force = lib.mkOption {
      type = types.ints.unsigned;
      default = 30;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
    cancel_ratio = lib.mkOption {
      type = types.float;
      default = 0.5;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
    create_new = lib.mkOption {
      type = types.bool;
      default = true;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
    forever = lib.mkOption {
      type = types.bool;
      default = false;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
    numbered = lib.mkOption {
      type = types.bool;
      default = false;
      description = lib.mdDoc ''description'';
      example = lib.literalExpression '''';
    };
  };
}
