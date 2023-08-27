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
  jq,
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
  enableNvidiaPatches ? false,
  enableXWayland ? true,
  legacyRenderer ? false,
  withSystemd ? lib.meta.availableOn stdenv.hostPlatform systemd,
  wrapRuntimeDeps ? true,
  version ? "git",
  commit,
  # deprecated flags
  nvidiaPatches ? false,
  hidpiXWayland ? false,
}:
assert lib.assertMsg (!nvidiaPatches) "The option `nvidiaPatches` has been renamed `enableNvidiaPatches`";
assert lib.assertMsg (!hidpiXWayland) "The option `hidpiXWayland` has been removed. Please refer https://wiki.hyprland.org/Configuring/XWayland";
  stdenv.mkDerivation {
    pname = "hyprland${lib.optionalString enableNvidiaPatches "-nvidia"}${lib.optionalString debug "-debug"}";
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
        libdrm
        libinput
        libxkbcommon
        mesa
        pango
        udis86
        wayland
        wayland-protocols
        pciutils
        (wlroots.override {inherit enableNvidiaPatches;})
      ]
      ++ lib.optionals enableXWayland [libxcb xcbutilwm xwayland]
      ++ lib.optionals withSystemd [systemd];

    mesonBuildType =
      if debug
      then "debug"
      else "release";

    mesonFlags = builtins.concatLists [
      ["-Dauto_features=disabled"]
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
      substituteInPlace meson.build \
        --replace "@GIT_COMMIT_HASH@" '${commit}' \
        --replace "@GIT_DIRTY@" '${
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
