{
  lib,
  stdenv,
  pkg-config,
  pkgconf,
  makeWrapper,
  cmake,
  ninja,
  binutils,
  cairo,
  expat,
  fribidi,
  git,
  hyprcursor,
  hyprlang,
  hyprwayland-scanner,
  jq,
  libGL,
  libdatrie,
  libdrm,
  libexecinfo,
  libinput,
  libselinux,
  libsepol,
  libthai,
  libuuid,
  libxkbcommon,
  mesa,
  pango,
  pciutils,
  pcre2,
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

      # Remove extra @PREFIX@ to fix pkg-config paths
      sed -i "s#@PREFIX@/##g" hyprland.pc.in
    '';

    DATE = date;
    HASH = commit;
    DIRTY = if commit == "" then "dirty" else "";

    nativeBuildInputs = lib.concatLists [
      [
        hyprwayland-scanner
        jq
        makeWrapper
        cmake
        ninja
        pkg-config
        python3
        wayland-scanner
      ]
      # introduce this later so that cmake takes precedence
      wlroots.nativeBuildInputs
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
        fribidi
        git
        hyprcursor.dev
        hyprlang
        libGL
        libdrm
        libdatrie
        libinput
        libselinux
        libsepol
        libthai
        libuuid
        libxkbcommon
        mesa
        pango
        pciutils
        pcre2
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
