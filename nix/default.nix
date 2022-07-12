{
  lib,
  stdenv,
  fetchFromGitHub,
  pkg-config,
  meson,
  ninja,
  libdrm,
  libinput,
  libxcb,
  libxkbcommon,
  mesa,
  mount,
  pango,
  wayland,
  wayland-protocols,
  wayland-scanner,
  wlroots,
  xcbutilwm,
  xwayland,
  enableXWayland ? true,
  version ? "git",
  debug ? false,
}:
stdenv.mkDerivation {
  pname = "hyprland" + lib.optionalString debug "-debug";
  inherit version;
  src = ../.;

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
  ];

  buildInputs =
    [
      libdrm
      libinput
      libxcb
      libxkbcommon
      mesa
      pango
      wayland
      wayland-protocols
      wayland-scanner
      (wlroots.override {inherit enableXWayland;})
      xcbutilwm
    ]
    ++ lib.optional enableXWayland xwayland;

  mesonBuildType =
    if debug
    then "debug"
    else "release";

  mesonFlags = lib.optional (!enableXWayland) "-DNO_XWAYLAND=true";

  patches = [
    # make meson use the provided wlroots instead of the git submodule
    ./meson-build.patch
  ];

  # Fix hardcoded paths to /usr installation
  postPatch = ''
    sed -i "s#/usr#$out#" src/render/OpenGL.cpp
  '';

  passthru.providedSessions = ["hyprland"];

  meta = with lib; {
    homepage = "https://github.com/vaxerski/Hyprland";
    description = "A dynamic tiling Wayland compositor that doesn't sacrifice on its looks";
    license = licenses.bsd3;
    platforms = platforms.linux;
    mainProgram = "Hyprland";
  };
}
