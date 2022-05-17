{
  description = "Hyprland is a dynamic tiling Wayland compositor that doesn't sacrifice on its looks";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    wlroots = {
      url = "gitlab:wlroots/wlroots?host=gitlab.freedesktop.org";
      flake = false;
    };
  };

  outputs = inputs @ {
    self,
    nixpkgs,
    ...
  }: let
    supportedSystems = [
      "aarch64-linux"
      "x86_64-linux"
    ];
    genSystems = nixpkgs.lib.genAttrs supportedSystems;
    pkgsFor = nixpkgs.legacyPackages;
  in {
    packages = genSystems (system: {
      wlroots = pkgsFor.${system}.wlroots.overrideAttrs (prev: {
        version = inputs.wlroots.lastModifiedDate;
        src = inputs.wlroots;
      });
      default = pkgsFor.${system}.callPackage ./default.nix {
        version = self.lastModifiedDate;
        inherit (self.packages.${system}) wlroots;
      };
    });

    formatter = genSystems (system: pkgsFor.${system}.alejandra);

    # TODO Provide a nixos module for easy installation and configuration
    # nixosModules.default = import ./module.nix;

    # Deprecated
    overlay = _: prev: {
      hyprland = self.packages.${prev.system}.default;
    };
  };
}
