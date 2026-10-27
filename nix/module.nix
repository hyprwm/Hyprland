inputs:
{
  config,
  lib,
  pkgs,
  ...
}:
let
  inherit (pkgs.stdenv.hostPlatform) system;
  selflib = import ./lib.nix lib;
  cfg = config.programs.hyprland;
in
{
  options = {
    programs.hyprland = {
      settings = lib.mkOption {
        type =
          with lib.types;
          let
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
        default = { };
        description = ''
          Hyprland configuration written in Nix. Entries with the same key
          should be written as lists. Variables' and colors' names should be
          quoted. See <https://wiki.hypr.land> for more examples.

          Special categories (e.g `devices`) should be written as
          `"devices[device-name]"`.

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
        default = [
          "$"
          "bezier"
        ];
        example = [
          "$"
          "bezier"
          "source"
        ];
        description = ''
          List of prefix of attributes to put at the top of the config.
        '';
      };

      bottomPrefixes = lib.mkOption {
        type = with lib.types; listOf str;
        default = [ ];
        example = [ "source" ];
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
      # Allows lua stub file to be accessed from /run/current-system/sw/share/hypr
      environment.pathsToLink = [ "/share/hypr" ];

      environment.etc."xdg/hypr/hyprland.conf" =
        let
          shouldGenerate = cfg.extraConfig != "" || cfg.settings != { };
        in
        lib.mkIf shouldGenerate {
          text =
            lib.optionalString (cfg.settings != { }) (
              selflib.toHyprlang {
                topCommandsPrefixes = cfg.topPrefixes;
                bottomCommandsPrefixes = cfg.bottomPrefixes;
              } cfg.settings
            )
            + lib.optionalString (cfg.extraConfig != "") cfg.extraConfig;
        };
    })
  ];
}
