self: {
  config,
  lib,
  pkgs,
  ...
}: let
  inherit (lib) types;
  cfg = config.wayland.windowManager.hyprland;
  defaultHyprlandPackage = self.packages.${pkgs.stdenv.hostPlatform.system}.default.override {
    enableXWayland = cfg.xwayland.enable;
    hidpiXWayland = cfg.xwayland.hidpi;
    nvidiaPatches = cfg.nvidiaPatches;
  };
in {
  options.wayland.windowManager.hyprland = {
    enable = lib.mkEnableOption "hyprland wayland compositor";

    package = lib.mkOption {
      type = types.nullOr types.package;
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
      type = types.bool;
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

    disableAutoreload = lib.mkOption {
      type = lib.types.bool;
      default = false;
      defaultText = lib.literalExpression "false";
      example = lib.literalExpression "true";
      description = ''
        Whether to disable automatically reloading Hyprland's configuration when
        rebuilding the Home Manager profile.
      '';
    };

    xwayland = {
      enable = lib.mkOption {
        type = types.bool;
        default = true;
        description = ''
          Enable XWayland.
        '';
      };
      hidpi = lib.mkOption {
        type = types.bool;
        default = false;
        description = ''
          Enable HiDPI XWayland.
        '';
      };
    };

    nvidiaPatches = lib.mkOption {
      type = lib.types.bool;
      default = false;
      defaultText = lib.literalExpression "false";
      example = lib.literalExpression "true";
      description = ''
        Patch wlroots for better Nvidia support.
      '';
    };

    config.general = {
      # NOTHING = lib.mkOption {
      #   type = ;
      #   default = ;
      #   description = lib.mdDoc ''

      #   '';
      #   # example = lib.literalExpression "";
      # };
      sensitivity = lib.mkOption {
        type = types.float;
        default = 1.0;
        description = lib.mdDoc ''
          **DEPRECATED**

          The sensitivity of all cursor devices.
          Do not use if specified in the *input* section.
        '';
        # example = lib.literalExpression "";
      };
      border_size = lib.mkOption {
        type = types.ints.positive;
        default = 1;
        description = lib.mdDoc ''
          The thickness of the window border, in pixels.
        '';
        # example = lib.literalExpression "";
      };
      no_border_on_floating = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc ''
          Disable borders for floating windows.
        '';
        # example = lib.literalExpression "";
      };
      gaps_inside = lib.mkOption {
        type = types.ints.positive;
        default = 5;
        description = lib.mdDoc ''
          Gap thickness between window edges, in pixels.

          *Renamed from: `gaps_in`*
        '';
        # example = lib.literalExpression "";
      };
      gaps_outside = lib.mkOption {
        type = types.ints.positive;
        default = 20;
        description = lib.mdDoc ''
          Padding on the perimeter of the monitor, in pixels.

          *Renamed from: `gaps_out`*
        '';
        # example = lib.literalExpression "";
      };
      inactive_border_color = lib.mkOption {
        type = types.singleLineStr;
        default = "0xFFFFFFFF";
        description = lib.mdDoc ''
          The color of the border on an inactive window, in 0xAARRGGBB format.

          *Renamed from: `col.inactive_border`*
        '';
        # example = lib.literalExpression "";
      };
      active_border_color = lib.mkOption {
        type = types.singleLineStr;
        default = "0xFFFFFFFF";
        description = lib.mdDoc ''
          The color of the border on an active window, in 0xAARRGGBB format.

          *Renamed from: `col.active_border`*
        '';
        # example = lib.literalExpression "";
      };
      cursor_inactive_timeout = lib.mkOption {
        type = types.ints.positive;
        default = 0;
        description = lib.mdDoc ''
          Duration of measured inactivity before hiding the cursor.

          Use `0` to disable.
        '';
        # example = lib.literalExpression "";
      };
      damage_tracking = lib.mkOption {
        type = types.enum ["none" "monitor" "full"];
        default = "full";
        description = lib.mdDoc ''
          **ADVANCED**

          Makes the compositor redraw only the changed pixels of the display.
          Saves on resources by not redrawing when not needed.

          Available modes: `none`, `monitor`, `full`

          You don't need to know what different modes do, just always use full.
        '';
        # example = lib.literalExpression "";
      };
      layout = lib.mkOption {
        type = types.enum ["dwindle" "master"];
        default = "dwindle";
        description = lib.mdDoc ''
          The layout algorithm to use.

          Available modes: `dwindle`, `master`
        '';
        # example = lib.literalExpression "";
      };
      no_cursor_warps = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc ''
          Whether to enable jumping the cursor when keyboard focus switches
          windows or similar event.
        '';
        # example = lib.literalExpression "";
      };
      apply_sens_to_raw = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc ''
          **ADVANCED**

          Apply the value of `sensitivity` to raw mouse input.
        '';
        # example = lib.literalExpression "";
      };
    };

    extraConfig = lib.mkOption {
      type = types.lines;
      default = "";
      description = lib.mdDoc ''
        Extra configuration lines to append to the bottom of
        `~/.config/hypr/hyprland.conf`.
      '';
    };

    recommendedEnvironment = lib.mkOption {
      type = types.bool;
      default = true;
      description = ''
        Whether to set the recommended environment variables.
      '';
      # example = lib.literalExpression "";
    };

    imports = [
      (
        lib.mkRenamedOptionModule
        ["wayland" "windowManager" "hyprland" "xwayland"]
        ["wayland" "windowManager" "hyprland" "xwayland" "enable"]
      )
    ];
  };

  config = lib.mkIf cfg.enable {
    home.packages =
      lib.optional (cfg.package != null) cfg.package
      ++ lib.optional cfg.xwayland.enable pkgs.xwayland;

    home.sessionVariables = lib.mkIf cfg.recommendedEnvironment {
      NIXOS_OZONE_WL = "1";
    };

    xdg.configFile."hypr/hyprland.conf" = {
      text = ''
        ### ENVIRONMENT INIT ###

        ${(lib.optionalString cfg.systemdIntegration ''
          exec-once=${pkgs.dbus}/bin/dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP
          exec-once=systemctl --user start hyprland-session.target
        '')}

        ### GENERAL ###

        ${with cfg.config.general; ''
          general {
            sensitivity = ${toString sensitivity}
            border_size =  ${toString border_size}
            no_border_on_floating = ${lib.boolToString no_border_on_floating}
            gaps_in = ${toString gaps_inside}
            gaps_out = ${toString gaps_outside}
            col.inactive_border = ${inactive_border_color}
            col.active_border = ${active_border_color}
            cursor_inactive_timeout = ${toString cursor_inactive_timeout}
            damage_tracking = ${toString damage_tracking}
            layout = ${toString layout}
            no_cursor_warps = ${lib.boolToString no_cursor_warps}
            apply_sens_to_raw = ${lib.boolToString apply_sens_to_raw}
          }
        ''}

        ### EXTRA CONFIG ###

        ${cfg.extraConfig}
      '';

      onChange = let
        hyprlandPackage =
          if cfg.package == null
          then defaultHyprlandPackage
          else cfg.package;
      in
        lib.mkIf (!cfg.disableAutoreload) ''          (  # execute in subshell so that `shopt` won't affect other scripts
                    shopt -s nullglob  # so that nothing is done if /tmp/hypr/ does not exist or is empty
                    for instance in /tmp/hypr/*; do
                      HYPRLAND_INSTANCE_SIGNATURE=''${instance##*/} ${hyprlandPackage}/bin/hyprctl reload config-only \
                        || true  # ignore dead instance(s)
                    done
                  )'';
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
