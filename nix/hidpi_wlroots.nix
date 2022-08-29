{
  wlroots,
  xwayland,
  fetchpatch,
}:
(wlroots.overrideAttrs
  (old: {
    patches =
      (old.patches or [])
      ++ [
        (fetchpatch {
          url = "https://gitlab.freedesktop.org/lilydjwg/wlroots/-/commit/6c5ffcd1fee9e44780a6a8792f74ecfbe24a1ca7.diff";
          sha256 = "sha256-Eo1pTa/PIiJsRZwIUnHGTIFFIedzODVf0ZeuXb0a3TQ=";
        })
        (fetchpatch {
          url = "https://gitlab.freedesktop.org/wlroots/wlroots/-/commit/18595000f3a21502fd60bf213122859cc348f9af.diff";
          sha256 = "sha256-jvfkAMh3gzkfuoRhB4E9T5X1Hu62wgUjj4tZkJm0mrI=";
          revert = true;
        })
      ];
  }))
.override {
  xwayland = xwayland.overrideAttrs (old: {
    patches =
      (old.patches or [])
      ++ [
        ./xwayland-vsync.patch
        ./xwayland-hidpi.patch
      ];
  });
}
