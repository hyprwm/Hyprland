{
  version,
  src,
  #
  wlroots,
  xwayland,
  fetchpatch,
  lib,
  hidpiXWayland ? true,
  enableXWayland ? true,
  nvidiaPatches ? false,
}:
assert (lib.assertMsg (hidpiXWayland -> enableXWayland) ''
  wlroots-hyprland: cannot have hidpiXWayland when enableXWayland is false.
'');
  (wlroots.overrideAttrs
    (old: {
      inherit version src;
      pname =
        old.pname
        + "-hyprland"
        + (
          if hidpiXWayland
          then "-hidpi"
          else ""
        )
        + (
          if nvidiaPatches
          then "-nvidia"
          else ""
        );
      patches =
        (old.patches or [])
        ++ (lib.optionals (enableXWayland && hidpiXWayland) [
          (fetchpatch {
            url = "https://gitlab.freedesktop.org/lilydjwg/wlroots/-/commit/6c5ffcd1fee9e44780a6a8792f74ecfbe24a1ca7.diff";
            sha256 = "sha256-Eo1pTa/PIiJsRZwIUnHGTIFFIedzODVf0ZeuXb0a3TQ=";
          })
          (fetchpatch {
            url = "https://gitlab.freedesktop.org/wlroots/wlroots/-/commit/18595000f3a21502fd60bf213122859cc348f9af.diff";
            sha256 = "sha256-jvfkAMh3gzkfuoRhB4E9T5X1Hu62wgUjj4tZkJm0mrI=";
            revert = true;
          })
        ]);
      postPatch =
        (old.postPatch or "")
        + (
          if nvidiaPatches
          then ''
            substituteInPlace render/gles2/renderer.c --replace "glFlush();" "glFinish();"
          ''
          else ""
        );
    }))
  .override {
    xwayland = xwayland.overrideAttrs (old: {
      patches =
        (old.patches or [])
        ++ (lib.optionals hidpiXWayland [
          ./xwayland-vsync.patch
          ./xwayland-hidpi.patch
        ]);
    });
  }
