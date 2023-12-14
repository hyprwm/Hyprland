self: {
  config,
  lib,
  pkgs,
  ...
}: let
  inherit (pkgs.stdenv.hostPlatform) system;

  package = self.packages.${system}.default;
in {
  config = {
    wayland.windowManager.hyprland.package = lib.mkDefault package;
  };
}
