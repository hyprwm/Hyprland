inputs: {
  config,
  lib,
  pkgs,
  ...
}: let
  inherit (pkgs.stdenv.hostPlatform) system;
  cfg = config.programs.hyprland;

  package = inputs.self.packages.${system}.hyprland;
  portalPackage = inputs.self.packages.${system}.xdg-desktop-portal-hyprland.override {
    hyprland = cfg.finalPackage;
  };

# basically 1:1 taken from https://github.com/nix-community/home-manager/blob/master/modules/services/window-managers/hyprland.nix
  toHyprconf = {
    attrs,
    indentLevel ? 0,
    importantPrefixes ? ["$"],
  }: let
    inherit
      (lib)
      all
      concatMapStringsSep
      concatStrings
      concatStringsSep
      filterAttrs
      foldl
      generators
      hasPrefix
      isAttrs
      isList
      mapAttrsToList
      replicate
      ;

    initialIndent = concatStrings (replicate indentLevel "  ");

    toHyprconf' = indent: attrs: let
      sections =
        filterAttrs (n: v: isAttrs v || (isList v && all isAttrs v)) attrs;

      mkSection = n: attrs:
        if lib.isList attrs
        then (concatMapStringsSep "\n" (a: mkSection n a) attrs)
        else ''
          ${indent}${n} {
          ${toHyprconf' "  ${indent}" attrs}${indent}}
        '';

      mkFields = generators.toKeyValue {
        listsAsDuplicateKeys = true;
        inherit indent;
      };

      allFields =
        filterAttrs (n: v: !(isAttrs v || (isList v && all isAttrs v)))
        attrs;

      isImportantField = n: _:
        foldl (acc: prev:
          if hasPrefix prev n
          then true
          else acc)
        false
        importantPrefixes;

      importantFields = filterAttrs isImportantField allFields;

      fields =
        builtins.removeAttrs allFields
        (mapAttrsToList (n: _: n) importantFields);
    in
      mkFields importantFields
      + concatStringsSep "\n" (mapAttrsToList mkSection sections)
      + mkFields fields;
  in
    toHyprconf' initialIndent attrs;
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

      sourceFirst =
        lib.mkEnableOption ''
          putting source entries at the top of the configuration
        ''
        // {
          default = true;
        };

      importantPrefixes = lib.mkOption {
        type = with lib.types; listOf str;
        default =
          ["$" "bezier" "name"]
          ++ lib.optionals cfg.sourceFirst ["source"];
        example = ["$" "bezier"];
        description = ''
          List of prefix of attributes to source at the top of the config.
        '';
      };
    };
  };
  config =
    {
      programs.hyprland = {
        package = lib.mkDefault package;
        portalPackage = lib.mkDefault portalPackage;
      };
    }
    // lib.mkIf cfg.enable {
      environment.systemPackages = lib.concatLists [
        (lib.optional (cfg.xwayland.enable) pkgs.xwayland)
      ];
      environment.etc."xdg/hypr/hyprland.conf" = let
        shouldGenerate = cfg.extraConfig != "" || cfg.settings != {} || cfg.plugins != [];

        pluginsToHyprconf = plugins:
          toHyprconf {
            attrs = {
              plugin = let
                mkEntry = entry:
                  if lib.types.package.check entry
                  then "${entry}/lib/lib${entry.pname}.so"
                  else entry;
              in
                map mkEntry cfg.plugins;
            };
            inherit (cfg) importantPrefixes;
          };
      in
        lib.mkIf shouldGenerate {
          text =
            lib.optionalString (cfg.plugins != [])
            (pluginsToHyprconf cfg.plugins)
            + lib.optionalString (cfg.settings != {})
            (toHyprconf {
              attrs = cfg.settings;
              inherit (cfg) importantPrefixes;
            })
            + lib.optionalString (cfg.extraConfig != "") cfg.extraConfig;
        };
    };
}
