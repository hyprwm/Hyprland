self: {
  config,
  lib,
  pkgs,
  ...
}: let
  cfg = config.wayland.windowManager.hyprland;
  defaultHyprlandPackage = self.packages.${pkgs.system}.default.override {
    enableXWayland = cfg.xwayland;
  };
in {
  options.wayland.windowManager.hyprland = {
    enable = lib.mkEnableOption "hyprland wayland compositor";
    package = lib.mkOption {
      type = with lib.types; nullOr package;
      default = defaultHyprlandPackage;
      description = ''
        Hyprland package to use. Will override the 'xwayland' option.

        Defaults to the one provided by the flake. Set it to
        <literal>pkgs.hyprland</literal> to use the one provided by nixpkgs or
        if you have an overlay.

        Set to null to not add any Hyprland package to your path. This should
        be done if you want to use the NixOS module to install Hyprland.
      '';
    };
    systemdIntegration = lib.mkOption {
      type = lib.types.bool;
      default = pkgs.stdenv.isLinux;
      description = ''
        Whether to enable <filename>hyprland-session.target</filename> on
        hyprland startup. This links to <filename>graphical-session.target</filename>.
        Some important environment variables will be imported to systemd
        and dbus user environment before reaching the target, including
        <itemizedlist>
          <listitem><para><literal>DISPLAY</literal></para></listitem>
          <listitem><para><literal>WAYLAND_DISPLAY</literal></para></listitem>
          <listitem><para><literal>HYPRLAND_INSTANCE_SIGNATURE</literal></para></listitem>
          <listitem><para><literal>XDG_CURRENT_DESKTOP</literal></para></listitem>
        </itemizedlist>
      '';
    };
    xwayland = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = ''
        Enable xwayland.
      '';
    };

    extraConfig = lib.mkOption {
      type = lib.types.lines;
      default = "";
      description = ''
        Extra configuration lines to add to ~/.config/hypr/hyprland.conf.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    home.packages =
      lib.optional (cfg.package != null) cfg.package
      ++ lib.optional cfg.xwayland pkgs.xwayland;

    xdg.configFile."hypr/hyprland.conf" = {
      text =
        (lib.optionalString cfg.systemdIntegration ''
            exec-once=export XDG_SESSION_TYPE=wayland
            exec-once=${pkgs.dbus}/bin/dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP
            exec-once=systemctl --user start hyprland-session.target
          '')
        + cfg.extraConfig;

      onChange = let
        hyprlandPackage =
          if cfg.package == null
          then defaultHyprlandPackage
          else cfg.package;
      in "HYPRLAND_INSTANCE_SIGNATURE=$(ls -w 1 /tmp/hypr | tail -1) ${hyprlandPackage}/bin/hyprctl reload";
    };

    systemd.user.targets.hyprland-session = lib.mkIf cfg.systemdIntegration {
      Unit = {
        Description = "hyprland compositor session";
        Documentation = ["man:systemd.special(7)"];
        BindsTo = ["graphical-session.target"];
        Wants = ["graphical-session-pre.target"];
        After = ["graphical-session-pre.target"];
      };
    };

    systemd.user.targets.tray = {
      Unit = {
        Description = "Home Manager System Tray";
        Requires = ["graphical-session-pre.target"];
      };
    };
  };
}
