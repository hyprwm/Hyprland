{
  self,
  lib,
  inputs,
}:
let
  mkDate =
    longDate:
    (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);
  ver = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in
{
  # Contains what a user is most likely to care about:
  # Hyprland itself, XDPH and the Share Picker.
  default = lib.composeManyExtensions (
    with self.overlays;
    [
      hyprland
      hyprland-extras
    ]
  );

  # Packages for variations of Hyprland, all dependencies included.
  hyprland-packages = lib.composeManyExtensions [
    # Dependencies
    inputs.aquamarine.overlays.default
    inputs.hyprcursor.overlays.default
    inputs.hyprgraphics.overlays.default
    inputs.hyprland-protocols.overlays.default
    inputs.hyprland-guiutils.overlays.default
    inputs.hyprtoolkit.overlays.default
    inputs.hyprlang.overlays.default
    inputs.hyprutils.overlays.default
    inputs.hyprwayland-scanner.overlays.default
    inputs.hyprwire.overlays.default
    # Hyprland packages themselves
    self.overlays.hyprland
  ];

  # BreakLand with its internal dependencies.
  hyprland = lib.composeManyExtensions (with self.overlays; [
    glaze
    hyprland-no-deps
  ]);

  # Hyprland without any dependencies.
  hyprland-no-deps =
    final: _prev:
    let
      date = mkDate (self.lastModifiedDate or "19700101");
      version = "${ver}+date=${date}_${self.shortRev or "dirty"}";
    in
    {
      hyprland = final.callPackage ./default.nix {
        stdenv = final.gcc16Stdenv;
        commit = self.rev or "";
        revCount = self.sourceInfo.revCount or "";
        inherit date version;
      };

      hyprland-unwrapped = final.hyprland.override { wrapRuntimeDeps = false; };

      hyprland-with-tests = final.hyprland.override { withTests = true; };

      # deprecated packages
      hyprland-legacy-renderer = builtins.trace ''
        hyprland-legacy-renderer was removed. Please use the hyprland package.
        Legacy renderer is no longer supported.
      '' final.hyprland;

      hyprland-nvidia = builtins.trace ''
        hyprland-nvidia was removed. Please use the hyprland package.
        Nvidia patches are no longer needed.
      '' final.hyprland;

      hyprland-hidpi = builtins.trace ''
        hyprland-hidpi was removed. Please use the hyprland package.
        For more information, refer to https://wiki.hypr.land/configuring/extra/xwayland/.
      '' final.hyprland;
    };

  # Debug
  hyprland-debug = lib.composeManyExtensions [
    # Dependencies
    self.overlays.hyprland-packages

    (_final: prev: {
      aquamarine = prev.aquamarine.override { debug = true; };
      hyprutils = prev.hyprutils.override { debug = true; };
      hyprland-debug = prev.hyprland.override { debug = true; };
    })
  ];

  # Packages for extra software recommended for usage with Hyprland,
  # including forked or patched packages for compatibility.
  hyprland-extras = lib.composeManyExtensions [
    inputs.xdph.overlays.default
  ];

  # Even though glaze itself disables it by default, nixpkgs sets ENABLE_SSL set to true.
  # Since we don't include openssl, the build failes without the `enableSSL = false;` override
  glaze = _final: prev: {
    glaze-hyprland = prev.glaze.override {
      enableSSL = false;
      enableInterop = false;
    };
  };
}
