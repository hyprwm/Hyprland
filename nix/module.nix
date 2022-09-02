# Copied from https://github.com/NixOS/nixpkgs/blob/master/nixos/modules/programs/sway.nix
self: {
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg = config.programs.hyprland;
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
      default = self.packages.${pkgs.system}.default;
      defaultText = literalExpression "<Hyprland flake>.packages.<system>.default";
      example = literalExpression "<Hyprland flake>.packages.<system>.default.override { }";
      description = ''
        Hyprland package to use.
      '';
    };
  };

  config = mkIf cfg.enable {
    environment.systemPackages = lib.optional (cfg.package != null) cfg.package;
    security.polkit.enable = true;
    hardware.opengl.enable = mkDefault true;
    fonts.enableDefaultFonts = mkDefault true;
    programs.dconf.enable = mkDefault true;
    services.xserver.displayManager.sessionPackages = lib.optional (cfg.package != null) cfg.package;
    programs.xwayland.enable = mkDefault true;
    xdg.portal.enable = mkDefault true;
    xdg.portal.extraPortals = [pkgs.xdg-desktop-portal-wlr];
  };
}
