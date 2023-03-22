# Copied from https://github.com/NixOS/nixpkgs/blob/master/nixos/modules/programs/sway.nix
inputs: {
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg = config.programs.hyprland;

  defaultHyprlandPackage = inputs.self.packages.${pkgs.stdenv.hostPlatform.system}.default.override {
    enableXWayland = cfg.xwayland.enable;
    hidpiXWayland = cfg.xwayland.hidpi;
    nvidiaPatches = cfg.nvidiaPatches;
  };
in {
  imports = [
    (mkRemovedOptionModule ["programs" "hyprland" "extraPackages"] "extraPackages has been removed. Use environment.systemPackages instead.")
  ];

  options.programs.hyprland = {
    enable = mkEnableOption ''
      Hyprland, the dynamic tiling Wayland compositor that doesn't sacrifice on its looks.
      You can manually launch Hyprland by executing "exec Hyprland" on a TTY.
      A configuration file will be generated in ~/.config/hypr/hyprland.conf.
      See <link xlink:href="https://github.com/vaxerski/Hyprland/wiki" /> for
      more information.
    '';

    package = mkOption {
      type = types.nullOr types.package;
      default = defaultHyprlandPackage;
      defaultText = literalExpression "<Hyprland flake>.packages.<system>.default";
      example = literalExpression "<Hyprland flake>.packages.<system>.default.override { }";
      description = ''
        Hyprland package to use.
      '';
    };

    xwayland = {
      enable = mkOption {
        type = types.bool;
        default = true;
        description = ''
          Enable XWayland.
        '';
      };
      hidpi = mkOption {
        type = types.bool;
        default = false;
        description = ''
          Enable HiDPI XWayland.
        '';
      };
    };

    nvidiaPatches = mkOption {
      type = types.bool;
      default = false;
      example = literalExpression "true";
      description = ''
        Patch wlroots for better Nvidia support.
      '';
    };

    recommendedEnvironment = mkOption {
      type = types.bool;
      default = true;
      defaultText = literalExpression "true";
      example = literalExpression "false";
      description = ''
        Whether to set the recommended environment variables.
      '';
    };
  };

  config = mkIf cfg.enable {
    environment = {
      systemPackages = lib.optional (cfg.package != null) cfg.package;

      sessionVariables = mkIf cfg.recommendedEnvironment {
        NIXOS_OZONE_WL = "1";
      };
    };
    fonts.enableDefaultFonts = mkDefault true;
    hardware.opengl.enable = mkDefault true;
    programs = {
      dconf.enable = mkDefault true;
      xwayland.enable = mkDefault true;
    };
    security.polkit.enable = true;
    services.xserver.displayManager.sessionPackages = lib.optional (cfg.package != null) cfg.package;
    xdg.portal = {
      enable = mkDefault true;
      # xdg-desktop-portal-hyprland
      extraPortals = lib.mkIf (cfg.package != null) [
        (inputs.xdph.packages.${pkgs.stdenv.hostPlatform.system}.xdg-desktop-portal-hyprland.override {
          hyprland-share-picker = inputs.xdph.packages.${pkgs.stdenv.hostPlatform.system}.hyprland-share-picker.override {
            hyprland = cfg.package;
          };
        })
      ];
    };
  };
}
