{
  self,
  lib,
  inputs,
}: let
  props = builtins.fromJSON (builtins.readFile ../props.json);

  mkDate = longDate: (lib.concatStringsSep "-" [
    (builtins.substring 0 4 longDate)
    (builtins.substring 4 2 longDate)
    (builtins.substring 6 2 longDate)
  ]);

  mkJoinedOverlays = overlays: final: prev:
    lib.foldl' (attrs: overlay: attrs // (overlay final prev)) {} overlays;
in {
  # Contains what a user is most likely to care about:
  # Hyprland itself, XDPH and the Share Picker.
  default = mkJoinedOverlays (with self.overlays; [
    hyprland-packages
    hyprland-extras
  ]);

  # Packages for variations of Hyprland, dependencies included.
  hyprland-packages = mkJoinedOverlays [
    # Dependencies
    inputs.hyprland-protocols.overlays.default
    self.overlays.wlroots-hyprland
    self.overlays.udis86
    # Hyprland packages themselves
    (final: prev: let
      date = mkDate (self.lastModifiedDate or "19700101");
    in {
      hyprland = final.callPackage ./default.nix {
        stdenv = final.gcc13Stdenv;
        version = "${props.version}+date=${date}_${self.shortRev or "dirty"}";
        wlroots = final.wlroots-hyprland;
        commit = self.rev or "";
        inherit (final) udis86 hyprland-protocols;
        inherit date;
      };
      hyprland-unwrapped = final.hyprland.override {wrapRuntimeDeps = false;};
      hyprland-debug = final.hyprland.override {debug = true;};
      hyprland-legacy-renderer = final.hyprland.override {legacyRenderer = true;};
      hyprland-nvidia =
        builtins.trace ''
          hyprland-nvidia was removed. Please use the hyprland package.
          Nvidia patches are no longer needed.
        ''
        final.hyprland;
      hyprland-hidpi =
        builtins.trace ''
          hyprland-hidpi was removed. Please use the hyprland package.
          For more information, refer to https://wiki.hyprland.org/Configuring/XWayland.
        ''
        final.hyprland;
    })
  ];

  # Packages for extra software recommended for usage with Hyprland,
  # including forked or patched packages for compatibility.
  hyprland-extras = mkJoinedOverlays [
    inputs.xdph.overlays.xdg-desktop-portal-hyprland
  ];

  udis86 = final: prev: {
    udis86 = final.callPackage ./udis86.nix {};
  };

  # Patched version of wlroots for Hyprland.
  # It is under a new package name so as to not conflict with
  # the standard version in nixpkgs.
  wlroots-hyprland = final: prev: {
    wlroots-hyprland = final.callPackage ./wlroots.nix {
      version = "${mkDate (inputs.wlroots.lastModifiedDate or "19700101")}_${inputs.wlroots.shortRev or "dirty"}";
      src = inputs.wlroots;
    };
  };
}
