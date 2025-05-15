{
  lib,
  stdenv,
  stdenvAdapters,
  pkg-config,
  pkgconf,
  makeWrapper,
  cmake,
  meson,
  ninja,
  aquamarine,
  binutils,
  cairo,
  epoll-shim,
  git,
  glaze,
  hyprcursor,
  hyprgraphics,
  hyprland-protocols,
  hyprland-qtutils,
  hyprlang,
  hyprutils,
  hyprwayland-scanner,
  libGL,
  libdrm,
  libexecinfo,
  libinput,
  libxkbcommon,
  libuuid,
  libgbm,
  pango,
  pciutils,
  re2,
  systemd,
  tomlplusplus,
  udis86-hyprland,
  wayland,
  wayland-protocols,
  wayland-scanner,
  xorg,
  xwayland,
  debug ? false,
  enableXWayland ? true,
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
  legacyRenderer ? false,
}: let
  inherit (builtins) baseNameOf foldl' readFile;
  inherit (lib.asserts) assertMsg;
  inherit (lib.attrsets) mapAttrsToList;
  inherit (lib.lists) flatten concatLists optional optionals;
  inherit (lib.sources) cleanSourceWith cleanSource;
  inherit (lib.strings) hasSuffix makeBinPath optionalString mesonBool mesonEnable trim;

  adapters = flatten [
    stdenvAdapters.useMoldLinker
    (lib.optional debug stdenvAdapters.keepDebugInfo)
  ];

  customStdenv = foldl' (acc: adapter: adapter acc) stdenv adapters;
in
  assert assertMsg (!nvidiaPatches) "The option `nvidiaPatches` has been removed.";
  assert assertMsg (!enableNvidiaPatches) "The option `enableNvidiaPatches` has been removed.";
  assert assertMsg (!hidpiXWayland) "The option `hidpiXWayland` has been removed. Please refer https://wiki.hyprland.org/Configuring/XWayland";
  assert assertMsg (!legacyRenderer) "The option `legacyRenderer` has been removed. Legacy renderer is no longer supported.";
    customStdenv.mkDerivation (finalAttrs: {
      pname = "hyprland${optionalString debug "-debug"}";
      inherit version;

      src = cleanSourceWith {
        filter = name: _type: let
          baseName = baseNameOf (toString name);
        in
          ! (hasSuffix ".nix" baseName);
        src = cleanSource ../.;
      };

      postPatch = ''
        # Fix hardcoded paths to /usr installation
        sed -i "s#/usr#$out#" src/render/OpenGL.cpp

        # Remove extra @PREFIX@ to fix pkg-config paths
        sed -i "s#@PREFIX@/##g" hyprland.pc.in
      '';

      COMMITS = revCount;
      DATE = date;
      DIRTY = optionalString (commit == "") "dirty";
      HASH = commit;
      TAG = "v${trim (readFile "${finalAttrs.src}/VERSION")}";

      depsBuildBuild = [
        pkg-config
      ];

      nativeBuildInputs = [
        hyprwayland-scanner
        makeWrapper
        meson
        ninja
        cmake # needed for glaze
        pkg-config
      ];

      outputs = [
        "out"
        "man"
        "dev"
      ];

      buildInputs = concatLists [
        [
          aquamarine
          cairo
          git
          glaze
          hyprcursor
          hyprgraphics
          hyprland-protocols
          hyprlang
          hyprutils
          libdrm
          libGL
          libinput
          libuuid
          libxkbcommon
          libgbm
          pango
          pciutils
          re2
          tomlplusplus
          udis86-hyprland
          wayland
          wayland-protocols
          wayland-scanner
          xorg.libXcursor
        ]
        (optionals customStdenv.hostPlatform.isBSD [epoll-shim])
        (optionals customStdenv.hostPlatform.isMusl [libexecinfo])
        (optionals enableXWayland [
          xorg.libxcb
          xorg.libXdmcp
          xorg.xcbutilerrors
          xorg.xcbutilrenderutil
          xorg.xcbutilwm
          xwayland
        ])
        (optional withSystemd systemd)
      ];

      strictDeps = true;

      mesonBuildType =
        if debug
        then "debug"
        else "release";

      mesonFlags = flatten [
        (mapAttrsToList mesonEnable {
          "xwayland" = enableXWayland;
          "systemd" = withSystemd;
          "uwsm" = false;
          "hyprpm" = false;
        })
        (mapAttrsToList mesonBool {
          "b_pch" = false;
          "tracy_enable" = false;
        })
      ];

      postInstall = ''
        ${optionalString wrapRuntimeDeps ''
          wrapProgram $out/bin/Hyprland \
            --suffix PATH : ${makeBinPath [
            binutils
            hyprland-qtutils
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
    })
