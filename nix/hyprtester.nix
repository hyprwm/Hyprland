{
  lib,
  stdenv,
  stdenvAdapters,
  cmake,
  pkg-config,
  hyprland,
  hyprwayland-scanner,
  version ? "git",
}: let
  inherit (lib.lists) flatten foldl';

  adapters = flatten [
    stdenvAdapters.useMoldLinker
    stdenvAdapters.keepDebugInfo
  ];

  customStdenv = foldl' (acc: adapter: adapter acc) stdenv adapters;
in
  customStdenv.mkDerivation (finalAttrs: {
    pname = "hyprtester";
    inherit version;

    src = ../.;

    nativeBuildInputs = [
      cmake
      pkg-config
      hyprwayland-scanner
    ];

    buildInputs = hyprland.buildInputs;

    preConfigure = ''
      cmake -S . -B ./build
      cmake --build ./build --target generate-protocol-headers -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

      cd hyprtester
    '';

    cmakeBuildType = "Debug";

    meta = {
      homepage = "https://github.com/hyprwm/Hyprland";
      description = "Hyprland testing framework";
      license = lib.licenses.bsd3;
      platforms = hyprland.meta.platforms;
      mainProgram = "hyprtester";
    };
  })
