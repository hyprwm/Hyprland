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
    inherit (cfg) nvidiaPatches;
  };
in {
  # disables Nixpkgs Hyprland module to avoid conflicts
  disabledModules = ["programs/hyprland.nix"];

  options.programs.hyprland = {
    enable =
      mkEnableOption null
      // {
        description = mdDoc ''
          Hyprland, the dynamic tiling Wayland compositor that doesn't sacrifice on its looks.

          You can manually launch Hyprland by executing {command}`Hyprland` on a TTY.

          A configuration file will be generated in {file}`~/.config/hypr/hyprland.conf`.
          See <https://wiki.hyprland.org> for more information.
        '';
      };

    package = mkOption {
      type = types.path;
      default = defaultHyprlandPackage;
      defaultText = literalExpression ''
        hyprland.packages.''${pkgs.stdenv.hostPlatform.system}.default.override {
          enableXWayland = config.programs.hyprland.xwayland.enable;
          hidpiXWayland = config.programs.hyprland.xwayland.hidpi;
          inherit (config.programs.hyprland) nvidiaPatches;
        }
      '';
      example = literalExpression "pkgs.hyprland";
      description = mdDoc ''
        The Hyprland package to use.
        Setting this option will make {option}`programs.hyprland.xwayland` and
        {option}`programs.hyprland.nvidiaPatches` not work.
      '';
    };

    xwayland = {
      enable = mkEnableOption (mdDoc "XWayland") // {default = true;};
      hidpi =
        mkEnableOption null
        // {
          description = mdDoc ''
            Enable HiDPI XWayland, based on [XWayland MR 733](https://gitlab.freedesktop.org/xorg/xserver/-/merge_requests/733).
            See <https://wiki.hyprland.org/Nix/Options-Overrides/#xwayland-hidpi> for more info.
          '';
        };
    };

    nvidiaPatches = mkEnableOption (mdDoc "patching wlroots for better Nvidia support");
  };

  config = mkIf cfg.enable {
    environment = {
      systemPackages = [cfg.package];

      sessionVariables = {
        NIXOS_OZONE_WL = mkDefault "1";
      };
    };

    fonts.enableDefaultFonts = mkDefault true;
    hardware.opengl.enable = mkDefault true;

    programs = {
      dconf.enable = mkDefault true;
      xwayland.enable = mkDefault true;
    };

    security.polkit.enable = true;

    services.xserver.displayManager.sessionPackages = [cfg.package];

    xdg.portal = {
      enable = mkDefault true;
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
