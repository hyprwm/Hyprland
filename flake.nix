# Based on fortuneteller2k's (https://github.com/fortuneteller2k/nixpkgs-f2k) package repo
{
  description =
    "Hyprland is a dynamic tiling Wayland compositor that doesn't sacrifice on its looks.";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-21.11";
    utils.url = "github:numtide/flake-utils";
    nixpkgs-wayland.url = "github:nix-community/nixpkgs-wayland";
    wlroots-git = {
      url = "gitlab:wlroots/wlroots?host=gitlab.freedesktop.org";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, utils, nixpkgs-wayland, wlroots-git }:
    {
      overlay = final: prev: {
        hyprland = prev.callPackage self {
          src = self;
          wlroots = (nixpkgs-wayland.overlays.default final prev).wlroots.overrideAttrs (prev: rec {
            src = wlroots-git;
          });
        };
      };
      overlays.default = self.overlay;
    } // utils.lib.eachSystem [ "aarch64-linux" "x86_64-linux" ] (system:
      let pkgs = nixpkgs.legacyPackages.${system};
      in rec {
        packages = {
          hyprland = pkgs.callPackage self {
            src = self;
            wlroots = nixpkgs-wayland.packages.${system}.wlroots.overrideAttrs (prev: rec {
            src = wlroots-git;
          });
          };
        };
        defaultPackage = packages.hyprland;
        apps.hyprland = utils.lib.mkApp { drv = packages.hyprland; };
        defaultApp = apps.hyprland;
        apps.default =
          utils.lib.mkApp { drv = self.packages.${system}.hyprland; };
      });
}
