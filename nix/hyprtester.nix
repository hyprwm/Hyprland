{
  lib,
  stdenv,
  cmake,
  pkg-config,
  hyprutils,
  version ? "git",
}:
stdenv.mkDerivation (finalAttrs: {
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

  meta = {
    homepage = "https://github.com/hyprwm/Hyprland";
    description = "Hyprland testing framework";
    license = lib.licenses.bsd3;
    platforms = hyprutils.meta.platforms;
    mainProgram = "hyprtester";
  };
})
