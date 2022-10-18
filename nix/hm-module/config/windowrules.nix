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
    description = lib.mdDoc ''
      List of sets containing:

       - `rules` = List of rules to apply to matched windows.
       - `class` = List of patterns to test the window class against.
       - `title` = List of patterns to test the window title against.

       See the example for more information.

       As an addendum, something you may want to use is this:

      ```nix
      let
        rule = {
          class ? null,
          title ? null,
        }: rules: {inherit class title rules;};
      in
        with patterns;
          lib.concatLists [
            (rule obsStudio ["size 1200 800" "workspace 10"])

            (map (rule ["float"]) [
              printerConfig
              audioControl
              bluetoothControl
              kvantumConfig
              filePickerPortal
              polkitAgent
              mountDialog
              calculator
              obsStudio
              steam
            ])
          ]
      ]
      ```
    '';
    example = lib.literalExpression ''
      let
        obsStudio = {
          class = ["com.obsproject.Studio"];
          title = ["OBS\s[\d\.]+.*"];
        };
        # match both WebCord and Discord clients
        # by two class names this will end up as
        # ^(WebCord|discord)$
        # in the config file.
        discord.class = ["WebCord" "discord"];
      in [
        # open OBS Studio on a specific workspace with an initial size
        (obsStudio // {rules = ["size 1200 800" "workspace 10"];})
        # make WebCord and Discord slightly transparent
        (discord // {rules = ["opacity 0.93 0.93"];})
      ]
    '';
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
    example = lib.literalExpression ''
      with patterns; [
        {
          rules = ["float"];
          group = [
            printerConfig
            audioControl
            bluetoothControl
            kvantumConfig
            filePickerPortal
            polkitAgent
            mountDialog
            firefoxExtension
            calculator
            obsStudio
            steam
          ];
        }
        {
          rules = ["opacity ${opacity.high} ${opacity.high}"];
          group = [
            discord
          ];
        }
        {
          rules = ["opacity ${opacity.mid} ${opacity.mid}"];
          group = [
            printerConfig
            audioControl
            bluetoothControl
            filePickerPortal
            vscode
            steam
          ];
        }
        {
          rules = ["opacity ${opacity.low} ${opacity.low}"];
          group = [
            calculator
          ];
        }
      ]
    '';
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
