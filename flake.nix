# Based on fortuneteller2k's (https://github.com/fortuneteller2k/nixpkgs-f2k) package repo
{
  description =
    "Hyprland is a dynamic tiling Wayland compositor that doesn't sacrifice on its looks.";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    utils.url = "github:numtide/flake-utils";
    nixpkgs-wayland.url = "github:nix-community/nixpkgs-wayland";
  };

  outputs = { self, nixpkgs, utils, nixpkgs-wayland }:
    {
      overlay = final: prev: {
        hyprland = prev.callPackage self {
          src = self;
          wlroots = (nixpkgs-wayland.overlay final prev).wlroots;
        };
      };
    } // utils.lib.eachDefaultSystem (system:
      let pkgs = nixpkgs.legacyPackages.${system};
      in rec {
        overlays = [ self.overlay ];
        packages = {
          hyprland = pkgs.callPackage self {
            src = self;
            inherit (nixpkgs-wayland.packages.${system}) wlroots;
          };
        };
        defaultPackage = packages.hyprland;
        apps.hyprland = utils.lib.mkApp { drv = packages.hyprland; };
        defaultApp = apps.hyprland;
        apps.default =
          utils.lib.mkApp { drv = self.packages."${system}".hyprland; };
      });
}
