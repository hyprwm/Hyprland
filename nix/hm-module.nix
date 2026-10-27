self:
{
  lib,
  pkgs,
  ...
}:
let
  inherit (pkgs.stdenv.hostPlatform) system;

  package = self.packages.${system}.default;
in
{
  config = {
    wayland.windowManager.breakland.package = lib.mkDefault package;
  };
}
