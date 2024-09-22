{
  description = "Hyprland is a dynamic tiling Wayland compositor that doesn't sacrifice on its looks";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # <https://github.com/nix-systems/nix-systems>
    systems.url = "github:nix-systems/default-linux";

    aquamarine = {
      url = "github:hyprwm/aquamarine";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
      inputs.hyprwayland-scanner.follows = "hyprwayland-scanner";
    };

    hyprcursor = {
      url = "github:hyprwm/hyprcursor";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprlang.follows = "hyprlang";
    };

    hyprland-protocols = {
      url = "github:hyprwm/hyprland-protocols";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprlang = {
      url = "github:hyprwm/hyprlang";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprutils = {
      url = "github:hyprwm/hyprutils";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprwayland-scanner = {
      url = "github:hyprwm/hyprwayland-scanner";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    xdph = {
      url = "github:hyprwm/xdg-desktop-portal-hyprland";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprlang.follows = "hyprlang";
      inputs.hyprutils.follows = "hyprutils";
      inputs.hyprwayland-scanner.follows = "hyprwayland-scanner";
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
    pkgsCrossFor = eachSystem (system: crossSystem:
      import nixpkgs {
        localSystem = system;
        crossSystem = crossSystem;
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
        hyprland-debug
        hyprland-legacy-renderer
        hyprland-unwrapped
        # hyprland-extras
        
        xdg-desktop-portal-hyprland
        ;
      hyprland-cross = (pkgsCrossFor.${system} "aarch64-linux").hyprland;
    });

    devShells = eachSystem (system: {
      default =
        pkgsFor.${system}.mkShell.override {
          inherit (self.packages.${system}.default) stdenv;
        } {
          name = "hyprland-shell";
          hardeningDisable = ["fortify"];
          inputsFrom = [pkgsFor.${system}.hyprland];
          packages = [pkgsFor.${system}.clang-tools];
        };
    });

    formatter = eachSystem (system: nixpkgs.legacyPackages.${system}.alejandra);

    nixosModules.default = import ./nix/module.nix inputs;
    homeManagerModules.default = import ./nix/hm-module.nix self;
  };
}
