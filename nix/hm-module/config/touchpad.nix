{
  lib,
  types,
}: {
  disable_while_typing = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  natural_scroll = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  clickfinger_behavior = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  middle_button_emulation = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  tap_to_click = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  drag_lock = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
  scroll_factor = lib.mkOption {
    type = types.float;
    default = 1.0;
    description = lib.mdDoc ''description'';
    example = lib.literalExpression '''';
  };
}
