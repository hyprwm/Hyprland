{
  lib,
  stdenv,
  pkg-config,
  makeWrapper,
  meson,
  ninja,
  binutils,
  cairo,
  git,
  hyprland-protocols,
  hyprlang,
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
  tomlplusplus,
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
  date,
  # deprecated flags
  enableNvidiaPatches ? false,
  nvidiaPatches ? false,
  hidpiXWayland ? false,
}:
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
        cairo
        git
        hyprland-protocols
        hyprlang
        libdrm
        libGL
        libinput
        libxkbcommon
        mesa
        pango
        pciutils
        tomlplusplus
        udis86
        wayland
        wayland-protocols
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
        --replace "@DATE@" "${date}" \
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
          --suffix PATH : ${lib.makeBinPath [
          stdenv.cc
          binutils
          pciutils
        ]}
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
