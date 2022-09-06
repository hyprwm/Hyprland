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
    environment = {
      systemPackages = lib.optional (cfg.package != null) cfg.package;
      sessionVariables = {
        CLUTTER_BACKEND = lib.mkDefault "wayland";
        GDK_BACKEND = lib.mkDefault "wayland";
        _JAVA_AWT_WM_NONREPARENTING = lib.mkDefault "1";
        MOZ_ENABLE_WAYLAND = lib.mkDefault "1";
        NIXOS_OZONE_WL = lib.mkDefault "1";
        QT_QPA_PLATFORM = lib.mkDefault "wayland;xcb";
        QT_WAYLAND_DISABLE_WINDOWDECORATION = lib.mkDefault "1";
        XCURSOR_SIZE = lib.mkDefault "24";
        XDG_SESSION_TYPE = lib.mkDefault "wayland";
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
      portal.extraPortals = [pkgs.xdg-desktop-portal-wlr];
    };
  };
}
