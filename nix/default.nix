{
  lib,
  stdenv,
  fetchFromGitHub,
  pkg-config,
  cmake,
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
}:
stdenv.mkDerivation {
  pname = "hyprland";
  inherit version;
  src = ../.;

  nativeBuildInputs = [
    cmake
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

  cmakeFlags =
    ["-DCMAKE_BUILD_TYPE=Release"]
    ++ lib.optional (!enableXWayland) "-DNO_XWAYLAND=true";

  # enables building with nix-supplied wlroots instead of submodule
  patches = [ ./wlroots.patch ];
  postPatch = ''
    make config
  '';

  postBuild = ''
    pushd ../hyprctl
    make all
    popd
  '';

  installPhase = ''
    pushd ..
    install -Dm644 ./example/hyprland.desktop -t $out/share/wayland-sessions
    install -Dm755 ./build/Hyprland -t $out/bin
    install -Dm755 ./hyprctl/hyprctl -t $out/bin
    install -Dm644 ./assets/* -t $out/share/hyprland
    install -Dm644 ./example/hyprland.conf -t $out/share/hyprland
    popd
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
