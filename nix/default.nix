{
  lib,
  stdenv,
  fetchFromGitHub,
  fetchpatch,
  pkg-config,
  meson,
  ninja,
  cairo,
  git,
  hyprland-protocols,
  jq,
  libdrm,
  libinput,
  libxcb,
  libxkbcommon,
  mesa,
  mount,
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
  hidpiXWayland ? true,
  legacyRenderer ? false,
  nvidiaPatches ? false,
  withSystemd ? true,
  version ? "git",
  commit,
}: let
  assertXWayland = lib.assertMsg (hidpiXWayland -> enableXWayland) ''
    Hyprland: cannot have hidpiXWayland when enableXWayland is false.
  '';
in
  assert assertXWayland;
    stdenv.mkDerivation {
      pname = "hyprland" + lib.optionalString debug "-debug";
      inherit version;

      src = lib.cleanSourceWith {
        filter = name: type: let
          baseName = baseNameOf (toString name);
        in
          ! (
            lib.hasSuffix ".nix" baseName
          );
        src = lib.cleanSource ../.;
      };

      nativeBuildInputs = [
        jq
        meson
        ninja
        pkg-config
      ];

      outputs = [
        "out"
        "man"
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
          udis86
          wayland
          wayland-protocols
          wayland-scanner
          pciutils
          (wlroots.override {inherit enableXWayland hidpiXWayland nvidiaPatches;})
        ]
        ++ lib.optionals enableXWayland [libxcb xcbutilwm xwayland]
        ++ lib.optionals withSystemd [systemd];

      mesonBuildType =
        if debug
        then "debug"
        else "release";

      mesonFlags = builtins.concatLists [
        (lib.optional (!enableXWayland) "-Dxwayland=disabled")
        (lib.optional legacyRenderer "-DLEGACY_RENDERER:STRING=true")
        (lib.optional withSystemd "-Dsystemd=enabled")
      ];

      patches = [
        # make meson use the provided wlroots instead of the git submodule
        ./meson-build.patch
      ];

      postPatch = ''
        # Fix hardcoded paths to /usr installation
        sed -i "s#/usr#$out#" src/render/OpenGL.cpp
        substituteInPlace meson.build \
          --replace "@GIT_COMMIT_HASH@" '${commit}' \
          --replace "@GIT_DIRTY@" '${if commit == "" then "dirty" else ""}'
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
