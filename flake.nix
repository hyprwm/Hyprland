{
  description = "Hyprland is a dynamic tiling Wayland compositor that doesn't sacrifice on its looks";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # <https://github.com/nix-systems/nix-systems>
    systems.url = "github:nix-systems/default-linux";

    wlroots = {
      type = "gitlab";
      host = "gitlab.freedesktop.org";
      owner = "wlroots";
      repo = "wlroots";
      rev = "47bf87ade2bd32395615a385ebde1fefbcdf79a2";
      flake = false;
    };

    hyprland-protocols = {
      url = "github:hyprwm/hyprland-protocols";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    xdph = {
      url = "github:hyprwm/xdg-desktop-portal-hyprland";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprland-protocols.follows = "hyprland-protocols";
    };
  };

  outputs = inputs @ {
    self,
    nixpkgs,
    systems,
    ...
  }: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);
    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem = system;
        overlays = with self.overlays; [
          hyprland-packages
          hyprland-extras
        ];
      });
  in {
    overlays = import ./nix/overlays.nix {inherit self lib inputs;};

    checks = eachSystem (system:
      (lib.filterAttrs
        (n: _: (lib.hasPrefix "hyprland" n) && !(lib.hasSuffix "debug" n))
        self.packages.${system})
      // {
        inherit (self.packages.${system}) xdg-desktop-portal-hyprland;
      });

    packages = eachSystem (system: {
      default = self.packages.${system}.hyprland;
      inherit
        (pkgsFor.${system})
        # hyprland-packages
        hyprland
        hyprland-unwrapped
        hyprland-debug
        hyprland-nvidia
        # hyprland-extras
        xdg-desktop-portal-hyprland
        # dependencies
        hyprland-protocols
        wlroots-hyprland
        udis86
        ;
    });

    devShells = eachSystem (system: {
      default = pkgsFor.${system}.mkShell.override {
        stdenv = pkgsFor.${system}.gcc13Stdenv;
      } {
        name = "hyprland-shell";
        nativeBuildInputs = with pkgsFor.${system}; [cmake python3];
        buildInputs = [self.packages.${system}.wlroots-hyprland];
        inputsFrom = [
          self.packages.${system}.wlroots-hyprland
          self.packages.${system}.hyprland
        ];
      };
    });

    formatter = eachSystem (system: nixpkgs.legacyPackages.${system}.alejandra);

    nixosModules.default = import ./nix/module.nix inputs;
    homeManagerModules.default = import ./nix/hm-module.nix self;
  };

  nixConfig = {
    extra-substituters = ["https://hyprland.cachix.org"];
    extra-trusted-public-keys = ["hyprland.cachix.org-1:a7pgxzMz7+chwVL3/pzj6jIBMioiJM7ypFP8PwtkuGc="];
  };
}
