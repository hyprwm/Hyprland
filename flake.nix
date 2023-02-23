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
    inherit (nixpkgs) lib;
    genSystems = lib.genAttrs [
      # Add more systems if they are supported
      "aarch64-linux"
      "x86_64-linux"
    ];

    pkgsFor = nixpkgs.legacyPackages;

    props = builtins.fromJSON (builtins.readFile ./props.json);

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
        version = props.version + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
        wlroots = wlroots-hyprland;
        inherit (inputs.hyprland-protocols.packages.${prev.stdenv.hostPlatform.system}) hyprland-protocols;
        inherit udis86;
      };
      hyprland-debug = hyprland.override {debug = true;};
      hyprland-no-hidpi = hyprland.override {hidpiXWayland = false;};
      hyprland-nvidia = hyprland.override {nvidiaPatches = true;};

      udis86 = prev.callPackage ./nix/udis86.nix {};

      waybar-hyprland = prev.waybar.overrideAttrs (oldAttrs: {
        postPatch = ''
          # use hyprctl to switch workspaces
          sed -i 's/zext_workspace_handle_v1_activate(workspace_handle_);/const std::string command = "hyprctl dispatch workspace " + name_;\n\tsystem(command.c_str());/g' src/modules/wlr/workspace_manager.cpp
        '';
        mesonFlags = oldAttrs.mesonFlags ++ ["-Dexperimental=true"];
      });

      xdg-desktop-portal-hyprland = inputs.xdph.packages.${prev.stdenv.hostPlatform.system}.default.override {
        hyprland-share-picker = inputs.xdph.packages.${prev.stdenv.hostPlatform.system}.hyprland-share-picker.override {inherit hyprland;};
      };
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

    nixosModules.default = import ./nix/module.nix inputs;
    homeManagerModules.default = import ./nix/hm-module.nix self;
  };

  nixConfig = {
    extra-substituters = ["https://hyprland.cachix.org"];
    extra-trusted-public-keys = ["hyprland.cachix.org-1:a7pgxzMz7+chwVL3/pzj6jIBMioiJM7ypFP8PwtkuGc="];
  };
}
