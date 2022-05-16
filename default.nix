{ lib, stdenv, fetchFromGitHub, src, pkg-config, cmake, ninja, libdrm, libinput
, libxcb, libxkbcommon, mesa, mount, pango, wayland, wayland-protocols
, wayland-scanner, wlroots, xcbutilwm, xwayland, enableXWayland ? true }:

stdenv.mkDerivation rec {
  pname = "hyprland";
  version = "git";
  inherit src;

  nativeBuildInputs = [ cmake ninja pkg-config wayland ]
    ++ lib.optional enableXWayland xwayland;

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
    (wlroots.override { inherit enableXWayland; })
    xcbutilwm
  ];

  cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ]
    ++ lib.optional (!enableXWayland) "-DNO_XWAYLAND=true";

  prePatch = ''
    make config
  '';

  postBuild = ''
    pushd ../hyprctl
    make all
    popd
  '';

  installPhase = ''
    cd ../
    mkdir -p $out/share/wayland-sessions
    cp ./example/hyprland.desktop $out/share/wayland-sessions
    mkdir -p $out/bin
    cp ./build/Hyprland $out/bin
    cp ./hyprctl/hyprctl $out/bin
    mkdir -p $out/share/hyprland
    cp ./assets/wall_2K.png $out/share/hyprland
    cp ./assets/wall_4K.png $out/share/hyprland
    cp ./assets/wall_8K.png $out/share/hyprland
  '';

  passthru.providedSessions = [ "hyprland" ];

  meta = with lib; {
    homepage = "https://github.com/vaxerski/Hyprland";
    description =
      "A dynamic tiling Wayland compositor that doesn't sacrifice on its looks";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
