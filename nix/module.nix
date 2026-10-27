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
  cfg = config.programs.breakland;
in
{
  options = {
    programs.breakland = {
      enable = lib.mkEnableOption "BreakLand";

      package = lib.mkOption {
        type = lib.types.package;
        default = inputs.self.packages.${system}.breakland;
        description = "The BreakLand package to install.";
      };

      portalPackage = lib.mkOption {
        type = lib.types.nullOr lib.types.package;
        default = inputs.self.packages.${system}.xdg-desktop-portal-breakland;
        description = "The BreakLand desktop portal package.";
      };

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
          description = "BreakLand configuration value";
              };
          in
          valueType;
        default = { };
        description = ''
          BreakLand configuration written in Nix. Entries with the same key
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
          Extra configuration lines to add to `/etc/xdg/breakland/breakland.conf`.
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
      programs.breakland = {
        package = lib.mkDefault inputs.self.packages.${system}.breakland;
        portalPackage = lib.mkDefault inputs.self.packages.${system}.xdg-desktop-portal-breakland;
      };
    }
    (lib.mkIf cfg.enable {
      # Allows BreakLand assets to be accessed from the system profile.
      environment.pathsToLink = [ "/share/breakland" ];

      environment.etc."xdg/breakland/breakland.conf" =
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
