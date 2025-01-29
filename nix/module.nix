inputs: {
  config,
  lib,
  pkgs,
  ...
}: let
  inherit (pkgs.stdenv.hostPlatform) system;
  cfg = config.programs.hyprland;

  toHyprlang = {
    topCommandsPrefixes ? ["$"],
    bottomCommandsPrefixes ? [],
  }: attrs: let
    inherit (pkgs) lib;
    inherit
      (lib.generators)
      mkKeyValueDefault
      toKeyValue
      ;
    inherit
      (lib.attrsets)
      filterAttrs
      isAttrs
      mapAttrsToList
      ;
    inherit
      (lib.lists)
      foldl
      isList
      ;
    inherit
      (lib.strings)
      concatMapStringsSep
      concatStringsSep
      hasPrefix
      ;
    inherit
      (lib.trivial)
      boolToString
      isBool
      ;
    inherit
      (builtins)
      all
      attrNames
      partition
      ;

    toHyprlang' = attrs: let
      toStr = x:
        if isBool x
        then boolToString x
        else toString x;

      categories = filterAttrs (n: v: isAttrs v || (isList v && all isAttrs v)) attrs;
      mkCategory = parent: attrs:
        if lib.isList attrs
        then concatMapStringsSep "\n" (a: mkCategory parent a) attrs
        else
          concatStringsSep "\n" (
            mapAttrsToList (
              k: v:
                if isAttrs v
                then mkCategory "${parent}:${k}" v
                else if isList v
                then concatMapStringsSep "\n" (item: "${parent}:${k} = ${toStr item}") v
                else "${parent}:${k} = ${toStr v}"
            )
            attrs
          );

      mkCommands = toKeyValue {
        mkKeyValue = mkKeyValueDefault {} " = ";
        listsAsDuplicateKeys = true;
        indent = "";
      };

      allCommands = filterAttrs (n: v: !(isAttrs v || (isList v && all isAttrs v))) attrs;

      filterCommands = list: n: foldl (acc: prefix: acc || hasPrefix prefix n) false list;

      # Get topCommands attr names
      result = partition (filterCommands topCommandsPrefixes) (attrNames allCommands);
      # Filter top commands from all commands
      topCommands = filterAttrs (n: _: (builtins.elem n result.right)) allCommands;
      # Remaining commands = allcallCommands - topCommands
      remainingCommands = removeAttrs allCommands result.right;

      # Get bottomCommands attr names
      result2 = partition (filterCommands bottomCommandsPrefixes) result.wrong;
      # Filter bottom commands from remainingCommands
      bottomCommands = filterAttrs (n: _: (builtins.elem n result2.right)) remainingCommands;
      # Regular commands = allCommands - topCommands - bottomCommands
      regularCommands = removeAttrs remainingCommands result2.right;
    in
      mkCommands topCommands
      + concatStringsSep "\n" (mapAttrsToList mkCategory categories)
      + mkCommands regularCommands
      + mkCommands bottomCommands;
  in
    toHyprlang' attrs;
in {
  options = {
    programs.hyprland = {
      plugins = lib.mkOption {
        type = with lib.types; listOf (either package path);
        default = [];
        description = ''
          List of Hyprland plugins to use. Can either be packages or
          absolute plugin paths.
        '';
      };

      settings = lib.mkOption {
        type = with lib.types; let
          valueType =
            nullOr (oneOf [
              bool
              int
              float
              str
              path
              (attrsOf valueType)
              (listOf valueType)
            ])
            // {
              description = "Hyprland configuration value";
            };
        in
          valueType;
        default = {};
        description = ''
          Hyprland configuration written in Nix. Entries with the same key
          should be written as lists. Variables' and colors' names should be
          quoted. See <https://wiki.hyprland.org> for more examples.

          Special categories (e.g `devices`) should be written as
          `"devices[device-name]"`.

          ::: {.note}
          Use the [](#programs.hyprland.plugins) option to
          declare plugins.
          :::

        '';
        example = lib.literalExpression ''
          {
            decoration = {
              shadow_offset = "0 5";
              "col.shadow" = "rgba(00000099)";
            };

            "$mod" = "SUPER";

            bindm = [
              # mouse movements
              "$mod, mouse:272, movewindow"
              "$mod, mouse:273, resizewindow"
              "$mod ALT, mouse:272, resizewindow"
            ];
          }
        '';
      };

      extraConfig = lib.mkOption {
        type = lib.types.lines;
        default = "";
        example = ''
          # window resize
          bind = $mod, S, submap, resize

          submap = resize
          binde = , right, resizeactive, 10 0
          binde = , left, resizeactive, -10 0
          binde = , up, resizeactive, 0 -10
          binde = , down, resizeactive, 0 10
          bind = , escape, submap, reset
          submap = reset
        '';
        description = ''
          Extra configuration lines to add to `/etc/xdg/hypr/hyprland.conf`.
        '';
      };

      topPrefixes = lib.mkOption {
        type = with lib.types; listOf str;
        default = ["$" "bezier"];
        example = ["$" "bezier" "source"];
        description = ''
          List of prefix of attributes to put at the top of the config.
        '';
      };

      bottomPrefixes = lib.mkOption {
        type = with lib.types; listOf str;
        default = [];
        example = ["source"];
        description = ''
          List of prefix of attributes to put at the bottom of the config.
        '';
      };
    };
  };
  config = lib.mkMerge [
    {
      programs.hyprland = {
        package = lib.mkDefault inputs.self.packages.${system}.hyprland;
        portalPackage = lib.mkDefault inputs.self.packages.${system}.xdg-desktop-portal-hyprland;
      };
    }
    (lib.mkIf cfg.enable {
      environment.etc."xdg/hypr/hyprland.conf" = let
        shouldGenerate = cfg.extraConfig != "" || cfg.settings != {} || cfg.plugins != [];

        pluginsToHyprlang = plugins:
          toHyprlang {
            topCommandsPrefixes = cfg.topPrefixes;
            bottomCommandsPrefixes = cfg.bottomPrefixes;
          }
          {
            plugin = let
              mkEntry = entry:
                if lib.types.package.check entry
                then "${entry}/lib/lib${entry.pname}.so"
                else entry;
            in
              map mkEntry cfg.plugins;
          };
      in
        lib.mkIf shouldGenerate {
          text =
            lib.optionalString (cfg.plugins != [])
            (pluginsToHyprlang cfg.plugins)
            + lib.optionalString (cfg.settings != {})
            (toHyprlang {
                topCommandsPrefixes = cfg.topPrefixes;
                bottomCommandsPrefixes = cfg.bottomPrefixes;
              }
              cfg.settings)
            + lib.optionalString (cfg.extraConfig != "") cfg.extraConfig;
        };
    })
  ];
}
