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
  options.programs.hyprland = {
    enable = mkEnableOption ''
      Hyprland, the dynamic tiling Wayland compositor that doesn't sacrifice on its looks.
      You can manually launch Hyprland by executing "exec Hyprland" on a TTY.
      A configuration file will be generated in ~/.config/hypr/hyprland.conf.
      See <link xlink:href="https://github.com/vaxerski/Hyprland/wiki" /> for
      more information.
    '';

    package = mkOption {
      type = types.package;
      default = self.packages.${pkgs.system}.default;
      defaultText = literalExpression "<Hyprland flake>.packages.<system>.default";
      example = literalExpression "<Hyprland flake>.packages.<system>.default.override { }";
      description = ''
        Hyprland package to use.
      '';
    };

    extraPackages = mkOption {
      type = with types; listOf package;
      default = with pkgs; [
        kitty
        wofi
        swaybg
      ];
      defaultText = literalExpression ''
        with pkgs; [ kitty wofi swaybg ];
      '';
      example = literalExpression ''
        with pkgs; [
          alacritty wofi
        ]
      '';
      description = ''
        Extra packages to be installed system wide.
      '';
    };
  };

  config = mkIf cfg.enable {
    environment.systemPackages = [cfg.package] ++ cfg.extraPackages;
    security.polkit.enable = true;
    hardware.opengl.enable = mkDefault true;
    fonts.enableDefaultFonts = mkDefault true;
    programs.dconf.enable = mkDefault true;
    services.xserver.displayManager.sessionPackages = [cfg.package];
    programs.xwayland.enable = mkDefault true;
    xdg.portal.enable = mkDefault true;
    xdg.portal.extraPortals = [pkgs.xdg-desktop-portal-wlr];
  };
}
