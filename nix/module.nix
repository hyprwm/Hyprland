inputs: {
  config,
  lib,
  pkgs,
  ...
}: let
  inherit (pkgs.stdenv.hostPlatform) system;
  cfg = config.programs.hyprland;

  package = inputs.self.packages.${system}.hyprland;
  portalPackage = inputs.self.packages.${system}.xdg-desktop-portal-hyprland.override {
    hyprland = cfg.finalPackage;
  };
in {
  config = {
    programs.hyprland = {
      package = lib.mkDefault package;
      portalPackage = lib.mkDefault portalPackage;
    };
  };
}
