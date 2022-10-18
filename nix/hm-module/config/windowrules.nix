{
  lib,
  types,
}: {
  rules = lib.mkOption {
    type = types.listOf types.attrs;
    default = [];
    description = lib.mdDoc '''';
    # example = lib.literalExpression '''';
    apply = windows: (lib.pipe windows [
      (map ({
        rules,
        class ? null,
        title ? null,
      }: (map (rule: {
          inherit rule;
          class =
            lib.mapNullable
            (x: "class:^(${lib.concatStringsSep "|" x})$")
            class;
          title =
            lib.mapNullable
            (x: "title:^(${lib.concatStringsSep "|" x})$")
            title;
        })
        rules)))
      lib.concatLists
      (map ({
        rule,
        class,
        title,
      }: "windowrulev2 = ${lib.concatStringsSep ", " (
        [rule]
        ++ (lib.optional (class != null) class)
        ++ (lib.optional (title != null) title)
      )}"))
    ]);
  };
}
