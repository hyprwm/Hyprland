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
    inherit (nixpkgs) lib;
    genSystems = lib.genAttrs [
      "x86_64-linux"
    ];
    pkgsFor = nixpkgs.legacyPackages;
    # https://github.com/NixOS/rfcs/pull/107
    mkVersion = longDate:
      lib.concatStrings [
        "0.pre"
        "+date="
        (lib.concatStringsSep "-" [
          (__substring 0 4 longDate)
          (__substring 4 2 longDate)
          (__substring 6 2 longDate)
        ])
      ];
  in {
    packages = genSystems (system: {
      wlroots = pkgsFor.${system}.wlroots.overrideAttrs (prev: {
        version = mkVersion inputs.wlroots.lastModifiedDate;
        src = inputs.wlroots;
      });
      default = pkgsFor.${system}.callPackage ./nix/default.nix {
        version = mkVersion self.lastModifiedDate;
        inherit (self.packages.${system}) wlroots;
      };
    });

    formatter = genSystems (system: pkgsFor.${system}.alejandra);

    nixosModules.default = import ./nix/module.nix self;

    # Deprecated
    overlays.default = _: prev: {
      hyprland = self.packages.${prev.system}.default;
    };
    overlay = self.overlays.default;
  };
}
