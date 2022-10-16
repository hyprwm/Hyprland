{
  lib,
  types,
}: {
  pass_mouse_when_bound = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  scroll_event_delay = lib.mkOption {
    type = types.ints.unsigned;
    default = 300; # TODO play with this
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  workspace_back_and_forth = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
  allow_workspace_cycles = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc '''';
    example = lib.literalExpression "";
  };
}
