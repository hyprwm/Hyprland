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
  withHyprtester ? false,
  # deprecated flags
  enableNvidiaPatches ? false,
  nvidiaPatches ? false,
  hidpiXWayland ? false,
  legacyRenderer ? false,
}: let
  inherit (builtins) foldl' readFile;
  inherit (lib.asserts) assertMsg;
  inherit (lib.attrsets) mapAttrsToList;
  inherit (lib.lists) flatten concatLists optional optionals;
  inherit (lib.strings) makeBinPath optionalString cmakeBool trim;
  fs = lib.fileset;

  adapters = flatten [
    stdenvAdapters.useMoldLinker
    (lib.optional debug stdenvAdapters.keepDebugInfo)
  ];

  customStdenv = foldl' (acc: adapter: adapter acc) stdenv adapters;
in
  assert assertMsg (!nvidiaPatches) "The option `nvidiaPatches` has been removed.";
  assert assertMsg (!enableNvidiaPatches) "The option `enableNvidiaPatches` has been removed.";
  assert assertMsg (!hidpiXWayland) "The option `hidpiXWayland` has been removed. Please refer https://wiki.hypr.land/Configuring/XWayland";
  assert assertMsg (!legacyRenderer) "The option `legacyRenderer` has been removed. Legacy renderer is no longer supported.";
    customStdenv.mkDerivation (finalAttrs: {
      pname = "hyprland${optionalString debug "-debug"}";
      inherit version;

      src = fs.toSource {
        root = ../.;
        fileset =
          fs.intersection
          # allows non-flake builds to only include files tracked by git
          (fs.gitTracked ../.)
          (fs.unions (flatten [
            ../assets/hyprland-portals.conf
            ../assets/install
            ../hyprctl
            ../hyprland.pc.in
            ../LICENSE
            ../protocols
            ../src
            ../systemd
            ../VERSION
            (fs.fileFilter (file: file.hasExt "1") ../docs)
            (fs.fileFilter (file: file.hasExt "conf" || file.hasExt "desktop") ../example)
            (fs.fileFilter (file: file.hasExt "sh") ../scripts)
            (fs.fileFilter (file: file.name == "CMakeLists.txt") ../.)
            (optional withHyprtester ../hyprtester)
          ]));
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
        cmake
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
        "NO_UWSM" = true;
        "NO_HYPRPM" = true;
        "TRACY_ENABLE" = false;
        "BUILD_HYPRTESTER" = withHyprtester;
      };

      preConfigure = ''
        substituteInPlace hyprtester/CMakeLists.txt --replace-fail \
          "\''${CMAKE_CURRENT_BINARY_DIR}" \
          "${placeholder "out"}/bin"
      '';

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
      '' + optionalString withHyprtester ''
        install hyprtester/pointer-warp -t $out/bin
        install hyprtester/pointer-scroll -t $out/bin
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
