{
  version,
  src,
  #
  wlroots,
  xwayland,
  fetchpatch,
  lib,
  hwdata,
  libliftoff,
  libdisplay-info,
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
          # adapted from https://gitlab.freedesktop.org/lilydjwg/wlroots/-/commit/6c5ffcd1fee9e44780a6a8792f74ecfbe24a1ca7
          ./wlroots-hidpi.patch
          (fetchpatch {
            url = "https://gitlab.freedesktop.org/wlroots/wlroots/-/commit/18595000f3a21502fd60bf213122859cc348f9af.diff";
            sha256 = "sha256-jvfkAMh3gzkfuoRhB4E9T5X1Hu62wgUjj4tZkJm0mrI=";
            revert = true;
          })
        ])
        ++ (lib.optionals nvidiaPatches [
          (fetchpatch {
            url = "https://aur.archlinux.org/cgit/aur.git/plain/0001-nvidia-format-workaround.patch?h=hyprland-nvidia-screenshare-git";
            sha256 = "A9f1p5EW++mGCaNq8w7ZJfeWmvTfUm4iO+1KDcnqYX8=";
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
      buildInputs = old.buildInputs ++ [hwdata libliftoff libdisplay-info];

      NIX_CFLAGS_COMPILE = toString [
        "-Wno-error=maybe-uninitialized"
      ];
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
