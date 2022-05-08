{ lib, stdenv, fetchFromGitHub, src, version, pkg-config, cmake, ninja, libdrm
, libinput, libxcb, libxkbcommon, mesa, mount, pango, wayland, wayland-protocols
, wayland-scanner, wlroots, xcbutilwm, xwayland, xwaylandSupport ? true }:

stdenv.mkDerivation {
  pname = "hyprland";
  inherit src version;

  nativeBuildInputs = [ cmake ninja pkg-config wayland ]
    ++ lib.optional xwaylandSupport xwayland;

  buildInputs = [
    libdrm
    libinput
    libxcb
    libxkbcommon
    mesa
    pango
    wayland-protocols
    wayland-scanner
    wlroots
    xcbutilwm
  ];

  dontBuild = true;
  dontInstall = true;

  postPatch = ''
    make install PREFIX=$out
  '';

  meta = with lib; {
    homepage = "https://github.com/vaxerski/Hyprland";
    description =
      "A dynamic tiling Wayland compositor that doesn't sacrifice on its looks";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
