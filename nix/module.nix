inputs: {
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg = config.programs.hyprland;

  finalPortalPackage = cfg.portalPackage.override {
    hyprland-share-picker = inputs.xdph.packages.${pkgs.system}.hyprland-share-picker.override {
      hyprland = cfg.finalPackage;
    };
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

    package = mkPackageOptionMD inputs.self.packages.${pkgs.system} "hyprland" { };

    finalPackage = mkOption {
      type = types.package;
      readOnly = true;
      default = cfg.package.override {
        enableXWayland = cfg.xwayland.enable;
        enableNvidiaPatches = cfg.enableNvidiaPatches;
      };
      defaultText =
        literalExpression
        "`programs.hyprland.package` with applied configuration";
      description = mdDoc ''
        The Hyprland package after applying configuration.
      '';
    };

    portalPackage = mkPackageOptionMD inputs.xdph.packages.${pkgs.system} "xdg-desktop-portal-hyprland" {};

    xwayland.enable = mkEnableOption (mdDoc "support for XWayland") // {default = true;};

    enableNvidiaPatches =
      mkEnableOption null
      // {
        description = mdDoc "Whether to apply patches to wlroots for better Nvidia support.";
      };
  };

  config = mkIf cfg.enable {
    environment.systemPackages = [cfg.finalPackage];

    fonts =
      if versionOlder config.system.stateVersion "23.11"
      then {enableDefaultFonts = mkDefault true;}
      else {enableDefaultPackages = mkDefault true;};

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
      mkRenamedOptionModule
      ["programs" "hyprland" "nvidiaPatches"]
      ["programs" "hyprland" "enableNvidiaPatches"]
    )
  ];
}
