{
  version,
  src,
  wlroots,
  enableXWayland ? true,
}:
wlroots.overrideAttrs (old: {
  inherit version src enableXWayland;

  pname = "${old.pname}-hyprland";

  patches = [ ]; # don't inherit old.patches
})
