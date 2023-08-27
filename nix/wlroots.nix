{
  lib,
  version,
  src,
  wlroots,
  hwdata,
  libdisplay-info,
  libliftoff,
  enableXWayland ? true,
  enableNvidiaPatches ? false,
}:
wlroots.overrideAttrs (old: {
  inherit version src enableXWayland;

  pname = "${old.pname}-hyprland${lib.optionalString enableNvidiaPatches "-nvidia"}";

  patches =
    (old.patches or [])
    ++ (lib.optionals enableNvidiaPatches [
      ./patches/wlroots-nvidia.patch
    ]);

  buildInputs = old.buildInputs ++ [hwdata libliftoff libdisplay-info];

  NIX_CFLAGS_COMPILE = toString [
    "-Wno-error=maybe-uninitialized"
  ];
})
