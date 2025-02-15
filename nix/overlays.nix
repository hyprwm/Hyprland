{
  self,
  lib,
  inputs,
}: let
  mkDate = longDate: (lib.concatStringsSep "-" [
    (builtins.substring 0 4 longDate)
    (builtins.substring 4 2 longDate)
    (builtins.substring 6 2 longDate)
  ]);
  ver = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in {
  # Contains what a user is most likely to care about:
  # Hyprland itself, XDPH and the Share Picker.
  default = lib.composeManyExtensions (with self.overlays; [
    hyprland-packages
    hyprland-extras
  ]);

  # Packages for variations of Hyprland, dependencies included.
  hyprland-packages = lib.composeManyExtensions [
    # Dependencies
    inputs.aquamarine.overlays.default
    inputs.hyprcursor.overlays.default
    inputs.hyprgraphics.overlays.default
    inputs.hyprland-protocols.overlays.default
    inputs.hyprland-qtutils.overlays.default
    inputs.hyprlang.overlays.default
    inputs.hyprutils.overlays.default
    inputs.hyprwayland-scanner.overlays.default
    self.overlays.udis86

    # Hyprland packages themselves
    (final: _prev: let
      date = mkDate (self.lastModifiedDate or "19700101");
      version = "${ver}+date=${date}_${self.shortRev or "dirty"}";
    in {
      hyprland = final.callPackage ./default.nix {
        stdenv = final.gcc15Stdenv;
        commit = self.rev or "";
        revCount = self.sourceInfo.revCount or "";
        inherit date version;
      };
      hyprland-unwrapped = final.hyprland.override {wrapRuntimeDeps = false;};

      # Build major libs with debug to get as much info as possible in a stacktrace
      hyprland-debug = final.hyprland.override {
        aquamarine = final.aquamarine.override {debug = true;};
        hyprutils = final.hyprutils.override {debug = true;};
        debug = true;
      };

      hyprtester = final.callPackage ./hyprtester.nix {
        inherit version;
      };

      # deprecated packages
      hyprland-legacy-renderer =
        builtins.trace ''
          hyprland-legacy-renderer was removed. Please use the hyprland package.
          Legacy renderer is no longer supported.
        ''
        final.hyprland;

      hyprland-nvidia =
        builtins.trace ''
          hyprland-nvidia was removed. Please use the hyprland package.
          Nvidia patches are no longer needed.
        ''
        final.hyprland;

      hyprland-hidpi =
        builtins.trace ''
          hyprland-hidpi was removed. Please use the hyprland package.
          For more information, refer to https://wiki.hypr.land/Configuring/XWayland.
        ''
        final.hyprland;
    })
  ];

  # Packages for extra software recommended for usage with Hyprland,
  # including forked or patched packages for compatibility.
  hyprland-extras = lib.composeManyExtensions [
    inputs.xdph.overlays.default
  ];

  # udis86 from nixpkgs is too old, and also does not provide a .pc file
  # this version is the one used in the git submodule, and allows us to
  # fetch the source without '?submodules=1'
  udis86 = final: prev: {
    udis86-hyprland = prev.udis86.overrideAttrs (_self: _super: {
      src = final.fetchFromGitHub {
        owner = "canihavesomecoffee";
        repo = "udis86";
        rev = "5336633af70f3917760a6d441ff02d93477b0c86";
        hash = "sha256-HifdUQPGsKQKQprByeIznvRLONdOXeolOsU5nkwIv3g=";
      };

      patches = [];
    });
  };
}
