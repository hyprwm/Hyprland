{
  lib,
  fetchurl,
  stdenv,
  pkg-config,
  makeWrapper,
  meson,
  ninja,
  binutils,
  cairo,
  git,
  hyprland-protocols,
  jq,
  libGL,
  libdrm,
  libinput,
  libxcb,
  libxkbcommon,
  mesa,
  pango,
  pciutils,
  systemd,
  udis86,
  wayland,
  wayland-protocols,
  wayland-scanner,
  wlroots,
  xcbutilwm,
  xwayland,
  debug ? false,
  enableXWayland ? true,
  legacyRenderer ? false,
  withSystemd ? lib.meta.availableOn stdenv.hostPlatform systemd,
  wrapRuntimeDeps ? true,
  version ? "git",
  commit,
  # deprecated flags
  enableNvidiaPatches ? false,
  nvidiaPatches ? false,
  hidpiXWayland ? false,
}:
let
  # NOTE: remove after https://github.com/NixOS/nixpkgs/pull/271096 reaches nixos-unstable
  libdrm_2_4_118 = libdrm.overrideAttrs(attrs: rec {
    version = "2.4.118";
    src = fetchurl {
      url = "https://dri.freedesktop.org/${attrs.pname}/${attrs.pname}-${version}.tar.xz";
      hash = "sha256-p3e9hfK1/JxX+IbIIFgwBXgxfK/bx30Kdp1+mpVnq4g=";
    };
  });
in
assert lib.assertMsg (!nvidiaPatches) "The option `nvidiaPatches` has been removed.";
assert lib.assertMsg (!enableNvidiaPatches) "The option `enableNvidiaPatches` has been removed.";
assert lib.assertMsg (!hidpiXWayland) "The option `hidpiXWayland` has been removed. Please refer https://wiki.hyprland.org/Configuring/XWayland";
  stdenv.mkDerivation {
    pname = "hyprland${lib.optionalString debug "-debug"}";
    inherit version;

    src = lib.cleanSourceWith {
      filter = name: type: let
        baseName = baseNameOf (toString name);
      in
        ! (lib.hasSuffix ".nix" baseName);
      src = lib.cleanSource ../.;
    };

    nativeBuildInputs = [
      jq
      meson
      ninja
      pkg-config
      makeWrapper
      wayland-scanner
    ];

    outputs = [
      "out"
      "man"
      "dev"
    ];

    buildInputs =
      [
        git
        cairo
        hyprland-protocols
        libGL
        libdrm_2_4_118
        libinput
        libxkbcommon
        mesa
        pango
        udis86
        wayland
        wayland-protocols
        pciutils
        wlroots
      ]
      ++ lib.optionals enableXWayland [libxcb xcbutilwm xwayland]
      ++ lib.optionals withSystemd [systemd];

    mesonBuildType =
      if debug
      then "debug"
      else "release";

    mesonAutoFeatures = "disabled";

    mesonFlags = builtins.concatLists [
      (lib.optional enableXWayland "-Dxwayland=enabled")
      (lib.optional legacyRenderer "-Dlegacy_renderer=enabled")
      (lib.optional withSystemd "-Dsystemd=enabled")
    ];

    patches = [
      # make meson use the provided wlroots instead of the git submodule
      ./patches/meson-build.patch
    ];

    postPatch = ''
      # Fix hardcoded paths to /usr installation
      sed -i "s#/usr#$out#" src/render/OpenGL.cpp

      # Generate version.h
      cp src/version.h.in src/version.h
      substituteInPlace src/version.h \
        --replace "@HASH@" '${commit}' \
        --replace "@BRANCH@" "" \
        --replace "@MESSAGE@" "" \
        --replace "@TAG@" "" \
        --replace "@DIRTY@" '${
        if commit == ""
        then "dirty"
        else ""
      }'
    '';

    postInstall = ''
      ln -s ${wlroots}/include/wlr $dev/include/hyprland/wlroots
      ${lib.optionalString wrapRuntimeDeps ''
        wrapProgram $out/bin/Hyprland \
          --suffix PATH : ${lib.makeBinPath [binutils pciutils]}
      ''}
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
