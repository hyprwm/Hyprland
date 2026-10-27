{
  description = "BreakLand is a focused tiling Wayland compositor";

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

    hyprgraphics = {
      url = "github:hyprwm/hyprgraphics";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprland-protocols = {
      url = "github:hyprwm/hyprland-protocols";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprland-guiutils = {
      url = "github:hyprwm/hyprland-guiutils";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.aquamarine.follows = "aquamarine";
      inputs.hyprgraphics.follows = "hyprgraphics";
      inputs.hyprtoolkit.follows = "hyprtoolkit";
      inputs.hyprutils.follows = "hyprutils";
      inputs.hyprlang.follows = "hyprlang";
      inputs.hyprwayland-scanner.follows = "hyprwayland-scanner";
    };

    hyprlang = {
      url = "github:hyprwm/hyprlang";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprtoolkit = {
      url = "github:hyprwm/hyprtoolkit";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.aquamarine.follows = "aquamarine";
      inputs.hyprgraphics.follows = "hyprgraphics";
      inputs.hyprlang.follows = "hyprlang";
      inputs.hyprutils.follows = "hyprutils";
      inputs.hyprwayland-scanner.follows = "hyprwayland-scanner";
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

    hyprwire = {
      url = "github:hyprwm/hyprwire";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    xdph = {
      url = "github:hyprwm/xdg-desktop-portal-hyprland";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.aquamarine.follows = "aquamarine";
      inputs.hyprgraphics.follows = "hyprgraphics";
      inputs.hyprland-protocols.follows = "hyprland-protocols";
      inputs.hyprlang.follows = "hyprlang";
      inputs.hyprtoolkit.follows = "hyprtoolkit";
      inputs.hyprutils.follows = "hyprutils";
      inputs.hyprwayland-scanner.follows = "hyprwayland-scanner";
    };

    pre-commit-hooks = {
      url = "github:cachix/git-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    inputs@{
      self,
      nixpkgs,
      systems,
      ...
    }:
    let
      inherit (nixpkgs) lib;
      eachSystem = lib.genAttrs (import systems);
      pkgsFor = eachSystem (
        system:
        import nixpkgs {
          localSystem = system;
          overlays = with self.overlays; [
             breakland-packages
             breakland-extras
          ];
        }
      );
      pkgsCrossFor = eachSystem (
        system: crossSystem:
        import nixpkgs {
          localSystem = system;
          inherit crossSystem;
          overlays = with self.overlays; [
             breakland-packages
             breakland-extras
          ];
        }
      );
      pkgsDebugFor = eachSystem (
        system:
        import nixpkgs {
          localSystem = system;
          overlays = with self.overlays; [
             breakland-debug
          ];
        }
      );
      pkgsDebugCrossFor = eachSystem (
        system: crossSystem:
        import nixpkgs {
          localSystem = system;
          inherit crossSystem;
          overlays = with self.overlays; [
             breakland-debug
          ];
        }
      );
    in
    {
      overlays = import ./nix/overlays.nix { inherit self lib inputs; };

      checks = eachSystem (
        system:
        (lib.filterAttrs (
           n: _: (lib.hasPrefix "breakland" n) && !(lib.hasSuffix "debug" n)
        ) self.packages.${system})
        // {
          inherit (self.packages.${system}) xdg-desktop-portal-breakland;
          pre-commit-check = inputs.pre-commit-hooks.lib.${system}.run {
            src = ./.;
            hooks = {
              breakland-treewide-formatter = {
                enable = true;
                entry = "${self.formatter.${system}}/bin/hyprland-treewide-formatter";
                pass_filenames = false;
                excludes = [ "subprojects" ];
                always_run = true;
              };
            };
          };
        }
        // (import ./nix/tests inputs pkgsFor.${system})
      );

      packages = eachSystem (system: {
        default = self.packages.${system}.breakland;
        inherit (pkgsFor.${system})
           breakland
           breakland-unwrapped
           breakland-with-tests
           xdg-desktop-portal-breakland
          ;
        inherit (pkgsDebugFor.${system}) breakland-debug;
        breakland-cross = (pkgsCrossFor.${system} "aarch64-linux").breakland;
        breakland-debug-cross = (pkgsDebugCrossFor.${system} "aarch64-linux").breakland-debug;
      });

      devShells = eachSystem (system: {
        default =
          pkgsFor.${system}.mkShell.override
            {
              inherit (self.packages.${system}.default) stdenv;
            }
            {
               name = "breakland-shell";
              hardeningDisable = [ "fortify" ];
               inputsFrom = [ pkgsFor.${system}.breakland ];
              packages = [ pkgsFor.${system}.clang-tools ];
              inherit (self.checks.${system}.pre-commit-check) shellHook;
            };
      });

      formatter = eachSystem (system: pkgsFor.${system}.callPackage ./nix/formatter.nix { });

      nixosModules.default = import ./nix/module.nix inputs;
      homeManagerModules.default = import ./nix/hm-module.nix self;

      # Hydra build jobs
      # Recent versions of Hydra can aggregate jobsets from 'hydraJobs' instead of a release.nix
      # or similar. Remember to filter large or incompatible attributes here. More eval jobs can
      # be added by merging, e.g., self.packages // self.devShells.
      hydraJobs = self.packages;
    };
}
