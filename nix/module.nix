inputs: {
  config,
  lib,
  pkgs,
  options,
  ...
}:
with lib; let
  cfg = config.programs.hyprland;
  inherit (pkgs.stdenv.hostPlatform) system;

  finalPortalPackage = cfg.portalPackage.override {
    hyprland = cfg.finalPackage;
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

    package = mkPackageOptionMD inputs.self.packages.${system} "hyprland" { };

    finalPackage = mkOption {
      type = types.package;
      readOnly = true;
      default = cfg.package.override {
        enableXWayland = cfg.xwayland.enable;
      };
      defaultText =
        literalExpression
        "`programs.hyprland.package` with applied configuration";
      description = mdDoc ''
        The Hyprland package after applying configuration.
      '';
    };

    portalPackage = mkPackageOptionMD inputs.xdph.packages.${system} "xdg-desktop-portal-hyprland" {};

    xwayland.enable = mkEnableOption (mdDoc "support for XWayland") // {default = true;};
  };

  config = mkIf cfg.enable {
    environment.systemPackages = [cfg.finalPackage];

    # NixOS changed the name of this attribute between NixOS 23.05 and
    # 23.11
    fonts = if builtins.hasAttr "enableDefaultPackages" options.fonts
      then {enableDefaultPackages = mkDefault true;}
      else {enableDefaultFonts = mkDefault true;};

    hardware.opengl.enable = mkDefault true;

    programs = {
      dconf.enable = mkDefault true;
      xwayland.enable = mkDefault cfg.xwayland.enable;
    };

    security.polkit.enable = true;

    services.xserver.displayManager.sessionPackages = [cfg.finalPackage];

    xdg.portal = {
      enable = mkDefault true;
      extraPortals = [finalPortalPackage];
    };
  };

  imports = with lib; [
    (
      mkRemovedOptionModule
      ["programs" "hyprland" "xwayland" "hidpi"]
      "XWayland patches are deprecated. Refer to https://wiki.hyprland.org/Configuring/XWayland"
    )
    (
      mkRemovedOptionModule
      ["programs" "hyprland" "enableNvidiaPatches"]
      "Nvidia patches are no longer needed"
    )
    (
      mkRemovedOptionModule
      ["programs" "hyprland" "nvidiaPatches"]
      "Nvidia patches are no longer needed"
    )
  ];
}
