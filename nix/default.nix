{
  lib,
  stdenv,
  stdenvAdapters,
  pkg-config,
  pkgconf,
  makeWrapper,
  cmake,
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
}: let
  inherit (builtins) baseNameOf foldl';
  inherit (lib.asserts) assertMsg;
  inherit (lib.attrsets) mapAttrsToList;
  inherit (lib.lists) flatten concatLists optional optionals;
  inherit (lib.sources) cleanSourceWith cleanSource;
  inherit (lib.strings) cmakeBool hasSuffix makeBinPath optionalString;

  adapters = flatten [
    stdenvAdapters.useMoldLinker
  ];

  customStdenv = foldl' (acc: adapter: adapter acc) stdenv adapters;
in
  assert assertMsg (!nvidiaPatches) "The option `nvidiaPatches` has been removed.";
  assert assertMsg (!enableNvidiaPatches) "The option `enableNvidiaPatches` has been removed.";
  assert assertMsg (!hidpiXWayland) "The option `hidpiXWayland` has been removed. Please refer https://wiki.hyprland.org/Configuring/XWayland";
    customStdenv.mkDerivation {
      pname = "hyprland${optionalString debug "-debug"}";
      inherit version;

      src = cleanSourceWith {
        filter = name: type: let
          baseName = baseNameOf (toString name);
        in
          ! (hasSuffix ".nix" baseName);
        src = cleanSource ../.;
      };

      patches = [
        # forces GCC to use -std=c++26
        ./stdcxx.patch

        # Nix does not have CMake 3.30 yet, so override the minimum version
        ./cmake-version.patch
      ];

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

      depsBuildBuild = [
        pkg-config
      ];

      nativeBuildInputs = [
        hyprwayland-scanner
        jq
        makeWrapper
        cmake
        pkg-config
        python3 # for udis86
        wayland-scanner
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
          hyprcursor
          hyprlang
          hyprutils
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
          xorg.libXcursor
        ]
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

      cmakeBuildType =
        if debug
        then "Debug"
        else "RelWithDebInfo";

      # we want as much debug info as possible
      dontStrip = debug;

      cmakeFlags = mapAttrsToList cmakeBool {
        "NO_XWAYLAND" = !enableXWayland;
        "LEGACY_RENDERER" = legacyRenderer;
        "NO_SYSTEMD" = !withSystemd;
        "CMAKE_DISABLE_PRECOMPILE_HEADERS" = true;
      };

      postInstall = ''
        ${optionalString wrapRuntimeDeps ''
          wrapProgram $out/bin/Hyprland \
            --suffix PATH : ${makeBinPath [
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
