{
  lib,
  stdenv,
  pkg-config,
  pkgconf,
  makeWrapper,
  meson,
  cmake,
  ninja,
  aquamarine,
  binutils,
  cairo,
  git,
  hyprcursor,
  hyprlang,
  hyprutils,
  hyprwayland-scanner,
  jq,
  libGL,
  libdrm,
  libexecinfo,
  libinput,
  libxkbcommon,
  libuuid,
  mesa,
  pango,
  pciutils,
  python3,
  systemd,
  tomlplusplus,
  wayland,
  wayland-protocols,
  wayland-scanner,
  xorg,
  xwayland,
  debug ? false,
  enableXWayland ? true,
  legacyRenderer ? false,
  withSystemd ? lib.meta.availableOn stdenv.hostPlatform systemd,
  wrapRuntimeDeps ? true,
  version ? "git",
  commit,
  revCount,
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

    postPatch = ''
      # Fix hardcoded paths to /usr installation
      sed -i "s#/usr#$out#" src/render/OpenGL.cpp

      # Remove extra @PREFIX@ to fix pkg-config paths
      sed -i "s#@PREFIX@/##g" hyprland.pc.in
    '';

    COMMITS = revCount;
    DATE = date;
    DIRTY = lib.optionalString (commit == "") "dirty";
    HASH = commit;

    depsBuildBuild = [
      pkg-config
    ];

    nativeBuildInputs = [
      hyprwayland-scanner
      jq
      makeWrapper
      meson
      cmake
      ninja
      pkg-config
      python3 # for udis86
      wayland-scanner
    ];

    outputs = [
      "out"
      "man"
      "dev"
    ];

    buildInputs = lib.concatLists [
      [
        aquamarine
        cairo
        # expat
        # fribidi
        git
        hyprcursor
        hyprlang
        hyprutils
        # libdatrie
        libdrm
        libGL
        libinput
        # libselinux
        # libsepol
        # libthai
        libuuid
        libxkbcommon
        mesa
        pango
        pciutils
        # pcre2
        tomlplusplus
        wayland
        wayland-protocols
        xorg.libXcursor
      ]
      (lib.optionals stdenv.hostPlatform.isMusl [libexecinfo])
      (lib.optionals enableXWayland [
        xorg.libxcb
        xorg.libXdmcp
        xorg.xcbutilerrors
        xorg.xcbutilrenderutil
        xorg.xcbutilwm
        xwayland
      ])
      (lib.optionals withSystemd [systemd])
    ];

    mesonBuildType =
      if debug
      then "debug"
      else "release";

    # we want as much debug info as possible
    dontStrip = debug;

    mesonFlags = [
      (lib.mesonEnable "xwayland" enableXWayland)
      (lib.mesonEnable "legacy_renderer" legacyRenderer)
      (lib.mesonEnable "systemd" withSystemd)
      "-Db_pch=false"
    ];

    postInstall = ''
      ${lib.optionalString wrapRuntimeDeps ''
        wrapProgram $out/bin/Hyprland \
          --suffix PATH : ${lib.makeBinPath [
          binutils
          pciutils
          pkgconf
        ]}
      ''}
    '';

    passthru.providedSessions = ["hyprland"];

    meta = {
      homepage = "https://github.com/hyprwm/Hyprland";
      description = "Dynamic tiling Wayland compositor that doesn't sacrifice on its looks";
      license = lib.licenses.bsd3;
      platforms = lib.platforms.linux;
      mainProgram = "Hyprland";
    };
  }
