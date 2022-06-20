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
      # Add more systems if they are supported
      "x86_64-linux"
    ];
    pkgsFor = nixpkgs.legacyPackages;
    mkDate = longDate: (lib.concatStringsSep "-" [
      (__substring 0 4 longDate)
      (__substring 4 2 longDate)
      (__substring 6 2 longDate)
    ]);
    pseudo-overlay = prev: rec {
      wlroots-hyprland = prev.wlroots.overrideAttrs (_: {
        version = mkDate (inputs.wlroots.lastModifiedDate or "19700101");
        src = inputs.wlroots;
      });
      hyprland = prev.callPackage ./nix/default.nix {
        version = "0.pre" + "+date=" + (mkDate (self.lastModifiedDate or "19700101"));
        wlroots = wlroots-hyprland;
      };
    };
  in {
    packages = genSystems (system:
      (pseudo-overlay pkgsFor.${system})
      // {
        default = self.packages.${system}.hyprland;
      });

    formatter = genSystems (system: pkgsFor.${system}.alejandra);

    nixosModules.default = import ./nix/module.nix self;

    overlays.default = final: pseudo-overlay;
    overlay = throw "Hyprland: .overlay output is deprecated, please use the .overlays.default output";
  };
}
