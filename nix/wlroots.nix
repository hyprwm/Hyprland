{
  fetchurl,
  version,
  src,
  wlroots,
  hwdata,
  libdisplay-info,
  libliftoff,
  libdrm,
  enableXWayland ? true,
}:
let
  # NOTE: remove after https://github.com/NixOS/nixpkgs/pull/271096 reaches nixos-unstable
  libdrm_2_4_118 = libdrm.overrideAttrs(old: rec {
    version = "2.4.118";
    src = fetchurl {
      url = "https://dri.freedesktop.org/${old.pname}/${old.pname}-${version}.tar.xz";
      hash = "sha256-p3e9hfK1/JxX+IbIIFgwBXgxfK/bx30Kdp1+mpVnq4g=";
    };
  });
in
wlroots.overrideAttrs (old: {
  inherit version src enableXWayland;

  pname = "${old.pname}-hyprland";

  # HACK: libdrm_2_4_118 is placed at the head of list to take precedence over libdrm in `old.buildInputs`
  buildInputs = [libdrm_2_4_118] ++ old.buildInputs ++ [hwdata libliftoff libdisplay-info];

  NIX_CFLAGS_COMPILE = toString [
    "-Wno-error=maybe-uninitialized"
  ];
})
