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
  inherit (lib.sources) cleanSourceWith cleanSource;
  inherit (lib.strings) hasSuffix cmakeBool;

  adapters = flatten [
    stdenvAdapters.useMoldLinker
    stdenvAdapters.keepDebugInfo
  ];

  customStdenv = foldl' (acc: adapter: adapter acc) stdenv adapters;
in
  customStdenv.mkDerivation (finalAttrs: {
    pname = "hyprtester";
    inherit version;

    src = cleanSourceWith {
      filter = name: _type: let
        baseName = baseNameOf (toString name);
      in
        ! (hasSuffix ".nix" baseName);
      src = cleanSource ../.;
    };

    nativeBuildInputs = [
      cmake
      pkg-config
      hyprwayland-scanner
    ];

    buildInputs = hyprland.buildInputs;

    preConfigure = ''
      cmake -S . -B .
      cmake --build . --target generate-protocol-headers -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

      cd hyprtester
    '';

    cmakeBuildType = "Debug";

    cmakeFlags = [(cmakeBool "TESTS" true)];

    meta = {
      homepage = "https://github.com/hyprwm/Hyprland";
      description = "Hyprland testing framework";
      license = lib.licenses.bsd3;
      platforms = hyprland.meta.platforms;
      mainProgram = "hyprtester";
    };
  })
