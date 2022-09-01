{
  lib,
  stdenv,
  fetchFromGitHub,
  fetchpatch,
  pkg-config,
  meson,
  ninja,
  git,
  libdrm,
  libinput,
  libxcb,
  libxkbcommon,
  mesa,
  mount,
  pango,
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
  version ? "git",
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
          libdrm
          libinput
          libxcb
          libxkbcommon
          mesa
          pango
          wayland
          wayland-protocols
          wayland-scanner
          (wlroots.override {inherit enableXWayland hidpiXWayland nvidiaPatches;})
          xcbutilwm
        ]
        ++ lib.optional enableXWayland xwayland;

      mesonBuildType =
        if debug
        then "debug"
        else "release";

      mesonFlags = builtins.concatLists [
        (lib.optional (!enableXWayland) "-Dxwayland=disabled")
        (lib.optional legacyRenderer "-DLEGACY_RENDERER:STRING=true")
      ];

      patches = [
        # make meson use the provided wlroots instead of the git submodule
        ./meson-build.patch
      ];

      # Fix hardcoded paths to /usr installation
      postPatch = ''
        sed -i "s#/usr#$out#" src/render/OpenGL.cpp
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
