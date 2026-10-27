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
  # Contains the BreakLand compositor and its desktop integration.
  default = lib.composeManyExtensions (
    with self.overlays;
    [
      breakland
      breakland-extras
    ]
  );

  # Packages for BreakLand and its dependencies.
  breakland-packages = lib.composeManyExtensions [
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
    # BreakLand packages themselves
    self.overlays.breakland
  ];

  # BreakLand with its internal dependencies.
  breakland = lib.composeManyExtensions (with self.overlays; [
    glaze
    breakland-no-deps
  ]);

  # BreakLand without any dependencies.
  breakland-no-deps =
    final: _prev:
    let
      date = mkDate (self.lastModifiedDate or "19700101");
      version = "${ver}+date=${date}_${self.shortRev or "dirty"}";
    in
    {
      breakland = final.callPackage ./default.nix {
        stdenv = final.gcc16Stdenv;
        commit = self.rev or "";
        revCount = self.sourceInfo.revCount or "";
        inherit date version;
      };

      breakland-unwrapped = final.breakland.override { wrapRuntimeDeps = false; };

      breakland-with-tests = final.breakland.override { withTests = true; };

      # deprecated packages
      breakland-legacy-renderer = builtins.trace ''
        breakland-legacy-renderer was removed. Please use the breakland package.
        Legacy renderer is no longer supported.
      '' final.breakland;

      breakland-nvidia = builtins.trace ''
        breakland-nvidia was removed. Please use the breakland package.
        Nvidia patches are no longer needed.
      '' final.breakland;

      breakland-hidpi = builtins.trace ''
        breakland-hidpi was removed. Please use the breakland package.
        For more information, refer to https://wiki.hypr.land/configuring/extra/xwayland/.
      '' final.breakland;
    };

  # Debug
  breakland-debug = lib.composeManyExtensions [
    # Dependencies
    self.overlays.breakland-packages

    (_final: prev: {
      aquamarine = prev.aquamarine.override { debug = true; };
      hyprutils = prev.hyprutils.override { debug = true; };
      breakland-debug = prev.breakland.override { debug = true; };
    })
  ];

  # Packages for extra software recommended for usage with BreakLand,
  # including forked or patched packages for compatibility.
  breakland-extras = lib.composeManyExtensions [
    inputs.xdph.overlays.default
    (_final: prev: {
      xdg-desktop-portal-breakland = prev.xdg-desktop-portal-hyprland;
    })
  ];

  # Even though glaze itself disables it by default, nixpkgs sets ENABLE_SSL set to true.
  # Since we don't include openssl, the build failes without the `enableSSL = false;` override
  glaze = _final: prev: {
    glaze-breakland = prev.glaze.override {
      enableSSL = false;
      enableInterop = false;
    };
  };
}
