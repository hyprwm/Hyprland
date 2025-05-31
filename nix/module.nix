inputs: {
  config,
  lib,
  pkgs,
  ...
}: let
  inherit (pkgs.stdenv.hostPlatform) system;
  selflib = import ./lib.nix lib;
  cfg = config.programs.hyprland;

  hyprlangType = with lib.types; let
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
        description = "Hyprlang configuration value";
      };
  in
    valueType;
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
        type = hyprlangType;
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

      defaultSettings = lib.mkOption {
        type = hyprlangType;
        default =
          {
            permission = [
              "${cfg.portalPackage}/libexec/.xdg-desktop-portal-hyprland-wrapped, screencopy, allow"
              "${lib.getExe pkgs.grim}, screencopy, allow"
            ];
          }
          // lib.mkIf cfg.withUWSM {
            exec-once = "uwsm finalize";
          };
        description = ''
          Default settings. Can be disabled by setting this option to `{}`.
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
          selflib.toHyprlang {
            topCommandsPrefixes = cfg.topPrefixes;
            bottomCommandsPrefixes = cfg.bottomPrefixes;
          }
          {
            "exec-once" = let
              mkEntry = entry:
                if lib.types.package.check entry
                then "${entry}/lib/lib${entry.pname}.so"
                else entry;
              hyprctl = lib.getExe' config.programs.hyprland.package "hyprctl";
            in
              map (p: "${hyprctl} plugin load ${mkEntry p}") cfg.plugins;
          };
      in
        lib.mkIf shouldGenerate {
          text =
            lib.optionalString (cfg.plugins != [])
            (pluginsToHyprlang cfg.plugins)
            + lib.optionalString (cfg.settings != {})
            (selflib.toHyprlang {
                topCommandsPrefixes = cfg.topPrefixes;
                bottomCommandsPrefixes = cfg.bottomPrefixes;
              }
              cfg.settings)
            + lib.optionalString (cfg.defaultSettings != {})
            (selflib.toHyprlang {
                topCommandsPrefixes = cfg.topPrefixes;
                bottomCommandsPrefixes = cfg.bottomPrefixes;
              }
              cfg.defaultSettings)
            + lib.optionalString (cfg.extraConfig != "") cfg.extraConfig;
        };
    })
  ];
}
