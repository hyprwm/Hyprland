{
  lib,
  stdenv,
  pkg-config,
  pkgconf,
  makeWrapper,
  cmake,
  meson,
  ninja,
  binutils,
  cairo,
  expat,
  git,
  hwdata,
  hyprcursor,
  hyprland-protocols,
  hyprlang,
  hyprwayland-scanner,
  jq,
  libGL,
  libdrm,
  libexecinfo,
  libinput,
  libuuid,
  libxkbcommon,
  mesa,
  pango,
  pciutils,
  python3,
  systemd,
  tomlplusplus,
  udis86,
  wayland,
  wayland-protocols,
  wayland-scanner,
  wlroots,
  xorg,
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

    nativeBuildInputs = [
      hyprwayland-scanner
      jq
      makeWrapper
      cmake
      meson
      ninja
      pkg-config
      python3
      wayland-scanner
    ];

    outputs = [
      "out"
      "man"
      "dev"
    ];

    buildInputs = lib.concatLists [
      wlroots.buildInputs
      udis86.buildInputs
      [
        cairo
        expat
        git
        hwdata
        hyprcursor.dev
        hyprland-protocols
        hyprlang
        libdrm
        libGL
        libinput
        libuuid
        libxkbcommon
        mesa
        pango
        pciutils
        tomlplusplus
        wayland
        wayland-protocols
      ]
      (lib.optionals stdenv.hostPlatform.isMusl [libexecinfo])
      (lib.optionals enableXWayland [
        xorg.libxcb
        xorg.libXdmcp
        xorg.xcbutil
        xorg.xcbutilwm
        xwayland
      ])
      (lib.optionals withSystemd [systemd])
    ];

    cmakeBuildType =
      if debug
      then "Debug"
      else "RelWithDebInfo";

    cmakeFlags = [
      (lib.cmakeBool "NO_XWAYLAND" (!enableXWayland))
      (lib.cmakeBool "LEGACY_RENDERER" legacyRenderer)
      (lib.cmakeBool "NO_SYSTEMD" (!withSystemd))
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

    meta = with lib; {
      homepage = "https://github.com/hyprwm/Hyprland";
      description = "A dynamic tiling Wayland compositor that doesn't sacrifice on its looks";
      license = licenses.bsd3;
      platforms = wlroots.meta.platforms;
      mainProgram = "Hyprland";
    };
  }
