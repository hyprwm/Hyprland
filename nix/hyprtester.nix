{
  lib,
  stdenv,
  stdenvAdapters,
  cmake,
  pkg-config,
  hyprutils,
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

    src = ../hyprtester;

    nativeBuildInputs = [
      cmake
      pkg-config
    ];

    buildInputs = [
      hyprutils
    ];

    cmakeBuildType = "Debug";

    meta = {
      homepage = "https://github.com/hyprwm/Hyprland";
      description = "Hyprland testing framework";
      license = lib.licenses.bsd3;
      platforms = hyprutils.meta.platforms;
      mainProgram = "hyprtester";
    };
  })
