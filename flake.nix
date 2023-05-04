{
  description = "Hyprland is a dynamic tiling Wayland compositor that doesn't sacrifice on its looks";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    wlroots = {
      url = "gitlab:wlroots/wlroots?host=gitlab.freedesktop.org";
      flake = false;
    };

    hyprland-protocols = {
      url = "github:hyprwm/hyprland-protocols";
      inputs.nixpkgs.follows = "nixpkgs";
    };

    xdph = {
      url = "github:hyprwm/xdg-desktop-portal-hyprland";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.hyprland-protocols.follows = "hyprland-protocols";
    };
  };

  outputs = inputs @ {
    self,
    nixpkgs,
    ...
  }: let
    lib = nixpkgs.lib.extend (import ./nix/lib.nix);
    genSystems = lib.genAttrs [
      # Add more systems if they are supported
      "aarch64-linux"
      "x86_64-linux"
    ];

    pkgsFor = genSystems (system:
      import nixpkgs {
        inherit system;
        overlays = [
          self.overlays.hyprland-packages
          self.overlays.wlroots-hyprland
          inputs.hyprland-protocols.overlays.default
        ];
      });
  in {
    overlays =
      (import ./nix/overlays.nix {inherit self lib inputs;})
      // {
        default =
          lib.mkJoinedOverlays
          (with self.overlays; [
            hyprland-packages
            hyprland-extras
            wlroots-hyprland
            inputs.hyprland-protocols.overlays.default
          ]);
      };

    checks = genSystems (system:
      (lib.filterAttrs
        (n: _: (lib.hasPrefix "hyprland" n) && !(lib.hasSuffix "debug" n))
        self.packages.${system})
      // {
        inherit (self.packages.${system}) xdg-desktop-portal-hyprland;
      });

    packages = genSystems (system:
      (self.overlays.default pkgsFor.${system} pkgsFor.${system})
      // {
        default = self.packages.${system}.hyprland;
      });

    devShells = genSystems (system: {
      default = pkgsFor.${system}.mkShell {
        name = "hyprland-shell";
        nativeBuildInputs = with pkgsFor.${system}; [cmake];
        buildInputs = [self.packages.${system}.wlroots-hyprland];
        inputsFrom = [
          self.packages.${system}.wlroots-hyprland
          self.packages.${system}.hyprland
        ];
      };
    });

    formatter = genSystems (system: pkgsFor.${system}.alejandra);

    nixosModules.default = import ./nix/module.nix inputs;
    homeManagerModules.default = import ./nix/hm-module.nix self;
  };

  nixConfig = {
    extra-substituters = ["https://hyprland.cachix.org"];
    extra-trusted-public-keys = ["hyprland.cachix.org-1:a7pgxzMz7+chwVL3/pzj6jIBMioiJM7ypFP8PwtkuGc="];
  };
}
