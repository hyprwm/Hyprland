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
      "aarch64-linux"
      "x86_64-linux"
    ];

    pkgsFor = genSystems (system:
      import nixpkgs {
        inherit system;
        overlays = [
          (_: prev: {
            libdrm = prev.libdrm.overrideAttrs (old: rec {
              version = "2.4.114";
              src = prev.fetchurl {
                url = "https://dri.freedesktop.org/${old.pname}/${old.pname}-${version}.tar.xz";
                sha256 = "sha256-MEnPhDpH0S5e7vvDvjSW14L6CfQjRr8Lfe/j0eWY0CY=";
              };
            });
            wayland-protocols = prev.wayland-protocols.overrideAttrs (old: rec {
              version = "1.29";
              src = prev.fetchurl {
                url = "https://gitlab.freedesktop.org/wayland/${old.pname}/-/releases/${version}/downloads/${old.pname}-${version}.tar.xz";
                hash = "sha256-4l6at1rHNnBN3v6S6PmshzC+q29WTbYvetaVu6T/ntg=";
              };
            });
          })
        ];
      });

    mkDate = longDate: (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);
  in {
    overlays.default = _: prev: rec {
      wlroots-hyprland = prev.callPackage ./nix/wlroots.nix {
        version = mkDate (inputs.wlroots.lastModifiedDate or "19700101") + "_" + (inputs.wlroots.shortRev or "dirty");
        src = inputs.wlroots;
      };
      hyprland = prev.callPackage ./nix/default.nix {
        stdenv = prev.gcc12Stdenv;
        version = "0.18.0beta" + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
        wlroots = wlroots-hyprland;
      };
      hyprland-debug = hyprland.override {debug = true;};
      hyprland-no-hidpi = hyprland.override {hidpiXWayland = false;};

      waybar-hyprland = prev.waybar.overrideAttrs (oldAttrs: {
        mesonFlags = oldAttrs.mesonFlags ++ ["-Dexperimental=true"];
      });
    };

    packages = genSystems (system:
      (self.overlays.default null pkgsFor.${system})
      // {
        default = self.packages.${system}.hyprland;
      });

    devShells = genSystems (system: {
      default = pkgsFor.${system}.mkShell.override {stdenv = pkgsFor.${system}.gcc12Stdenv;} {
        name = "hyprland-shell";
        nativeBuildInputs = with pkgsFor.${system}; [
          cmake
        ];
        buildInputs = [
          self.packages.${system}.wlroots-hyprland
        ];
        inputsFrom = [
          self.packages.${system}.wlroots-hyprland
          self.packages.${system}.hyprland
        ];
      };
    });

    formatter = genSystems (system: pkgsFor.${system}.alejandra);

    nixosModules.default = import ./nix/module.nix self;
    homeManagerModules.default = import ./nix/hm-module.nix self;
  };

  nixConfig = {
    extra-substituters = ["https://hyprland.cachix.org"];
    extra-trusted-public-keys = ["hyprland.cachix.org-1:a7pgxzMz7+chwVL3/pzj6jIBMioiJM7ypFP8PwtkuGc="];
  };
}
