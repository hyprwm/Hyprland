self: {
  config,
  lib,
  pkgs,
  ...
}: let
  cfg = config.wayland.windowManager.hyprland;
  defaultHyprlandPackage = self.packages.${pkgs.stdenv.hostPlatform.system}.default.override {
    enableXWayland = cfg.xwayland.enable;
    inherit (cfg) enableNvidiaPatches;
  };
in {
  disabledModules = ["services/window-managers/hyprland.nix"];

  meta.maintainers = [lib.maintainers.fufexan];

  options.wayland.windowManager.hyprland = {
    enable =
      lib.mkEnableOption null
      // {
        description = lib.mdDoc ''
          Whether to enable Hyprland, the dynamic tiling Wayland compositor
          that doesn't sacrifice on its looks.

          You can manually launch Hyprland by executing {command}`Hyprland` on
          a TTY.

          See <https://wiki.hyprland.org> for more information.
        '';
      };

    package = lib.mkOption {
      type = with lib.types; nullOr package;
      default = defaultHyprlandPackage;
      defaultText = lib.literalExpression ''
        hyprland.packages.''${pkgs.stdenv.hostPlatform.system}.default.override {
          enableXWayland = config.wayland.windowManager.hyprland.xwayland.enable;
          inherit (config.wayland.windowManager.hyprland) enableNvidiaPatches;
        }
      '';
      description = lib.mdDoc ''
        Hyprland package to use. Will override the 'xwayland' and
        'enableNvidiaPatches' options.

        Defaults to the one provided by the flake. Set it to
        {package}`pkgs.hyprland` to use the one provided by nixpkgs or
        if you have an overlay.

        Set to null to not add any Hyprland package to your path. This should
        be done if you want to use the NixOS module to install Hyprland.
      '';
    };

    plugins = lib.mkOption {
      type = with lib.types; listOf (either package path);
      default = [];
      description = lib.mdDoc ''
        List of Hyprland plugins to use. Can either be packages or
        absolute plugin paths.
      '';
    };

    systemdIntegration = lib.mkOption {
      type = lib.types.bool;
      default = pkgs.stdenv.isLinux;
      description = lib.mdDoc ''
        Whether to enable {file}`hyprland-session.target` on
        Hyprland startup. This links to {file}`graphical-session.target`.
        Some important environment variables will be imported to systemd
        and dbus user environment before reaching the target, including
        - {env}`DISPLAY`
        - {env}`HYPRLAND_INSTANCE_SIGNATURE`
        - {env}`WAYLAND_DISPLAY`
        - {env}`XDG_CURRENT_DESKTOP`
      '';
    };

    disableAutoreload =
      lib.mkEnableOption null
      // {
        description = lib.mdDoc ''
          Whether to disable automatically reloading Hyprland's configuration when
          rebuilding the Home Manager profile.
        '';
      };

    xwayland.enable = lib.mkEnableOption (lib.mdDoc "XWayland") // {default = true;};

    enableNvidiaPatches = lib.mkEnableOption (lib.mdDoc "patching wlroots for better Nvidia support.");

    extraConfig = lib.mkOption {
      type = lib.types.nullOr lib.types.lines;
      default = "";
      description = lib.mdDoc ''
        Extra configuration lines to add to {file}`~/.config/hypr/hyprland.conf`.
      '';
    };

    recommendedEnvironment =
      lib.mkEnableOption null
      // {
        description = lib.mdDoc ''
          Whether to set the recommended environment variables.
        '';
      };
  };

  config = lib.mkIf cfg.enable {
    warnings =
      if (cfg.systemdIntegration || cfg.plugins != []) && cfg.extraConfig == null
      then [
        ''
          You have enabled hyprland.systemdIntegration or listed plugins in hyprland.plugins.
          Your Hyprland config will be linked by home manager.
          Set hyprland.extraConfig or unset hyprland.systemdIntegration and hyprland.plugins to remove this warning.
        ''
      ]
      else [];

    home.packages =
      lib.optional (cfg.package != null) cfg.package
      ++ lib.optional cfg.xwayland.enable pkgs.xwayland;

    home.sessionVariables =
      lib.mkIf cfg.recommendedEnvironment {NIXOS_OZONE_WL = "1";};

    xdg.configFile."hypr/hyprland.conf" = lib.mkIf (cfg.systemdIntegration || cfg.extraConfig != null || cfg.plugins != []) {
      text =
        (lib.optionalString cfg.systemdIntegration ''
          exec-once=${pkgs.dbus}/bin/dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP && systemctl --user start hyprland-session.target
        '')
        + lib.concatStrings (builtins.map (entry: let
          plugin =
            if lib.types.package.check entry
            then "${entry}/lib/lib${entry.pname}.so"
            else entry;
        in "plugin = ${plugin}\n")
        cfg.plugins)
        + (
          if cfg.extraConfig != null
          then cfg.extraConfig
          else ""
        );

      onChange = let
        hyprlandPackage =
          if cfg.package == null
          then defaultHyprlandPackage
          else cfg.package;
      in
        lib.mkIf (!cfg.disableAutoreload) ''
          (  # execute in subshell so that `shopt` won't affect other scripts
            shopt -s nullglob  # so that nothing is done if /tmp/hypr/ does not exist or is empty
            for instance in /tmp/hypr/*; do
              HYPRLAND_INSTANCE_SIGNATURE=''${instance##*/} ${hyprlandPackage}/bin/hyprctl reload config-only \
                || true  # ignore dead instance(s)
            done
          )
        '';
    };

    systemd.user.targets.hyprland-session = lib.mkIf cfg.systemdIntegration {
      Unit = {
        Description = "Hyprland compositor session";
        Documentation = ["man:systemd.special(7)"];
        BindsTo = ["graphical-session.target"];
        Wants = ["graphical-session-pre.target"];
        After = ["graphical-session-pre.target"];
      };
    };
  };

  imports = [
    (lib.mkRemovedOptionModule ["wayland" "windowManager" "hyprland" "xwayland" "hidpi"]
      "Support for this option has been removed. Refer to https://wiki.hyprland.org/Configuring/XWayland for more info")
  ];
}
