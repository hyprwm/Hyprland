{
  lib,
  types,
}: let
  expandRules = map ({
    rules,
    class ? null,
    title ? null,
  }: (
    map (rule: {
      inherit rule class title;
    })
    rules
  ));

  compileRules = map ({
    rule,
    class,
    title,
  }: {
    inherit rule;
    class =
      lib.mapNullable
      (x: "class:^(${lib.concatStringsSep "|" x})$")
      class;
    title =
      lib.mapNullable
      (x: "title:^(${lib.concatStringsSep "|" x})$")
      title;
  });

  stringifyRules = map ({
    rule,
    class,
    title,
  }: "windowrulev2 = ${lib.concatStringsSep ", " (
    [rule]
    ++ (lib.optional (class != null) class)
    ++ (lib.optional (title != null) title)
  )}");

  expandRuleGroups = map ({
    rules,
    group,
  }:
    map (window: window // {inherit rules;}) group);
in {
  rules = lib.mkOption {
    type = types.listOf types.attrs;
    default = [];
    description = lib.mdDoc '''';
    # example = lib.literalExpression '''';
    apply = windows:
      lib.pipe windows [
        expandRules
        lib.concatLists
        compileRules
        stringifyRules
        (lib.concatStringsSep "\n")
      ];
  };
  ruleGroups = lib.mkOption {
    type = types.listOf types.attrs;
    default = [];
    description = lib.mdDoc '''';
    # example = lib.literalExpression '''';
    apply = groups:
      lib.pipe groups [
        expandRuleGroups
        lib.concatLists
        expandRules
        lib.concatLists
        compileRules
        stringifyRules
        (lib.concatStringsSep "\n")
      ];
  };
}
