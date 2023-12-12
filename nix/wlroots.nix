{
  version,
  src,
  wlroots,
  hwdata,
  libdisplay-info,
  libliftoff,
  enableXWayland ? true,
}:
wlroots.overrideAttrs (old: {
  inherit version src enableXWayland;

  pname = "${old.pname}-hyprland";

  buildInputs =
    old.buildInputs
    ++ [
      hwdata
      libliftoff
      libdisplay-info
    ];
})
