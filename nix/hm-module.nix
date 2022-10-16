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
    enable = lib.mkEnableOption (lib.mdDoc ''
      ${cfg.package.meta.description}

      <https://wiki.hyprland.org>
    '');

    ### NIX ###

    package = lib.mkOption {
      type = types.nullOr types.package;
      default = defaultHyprlandPackage;
      description = lib.mdDoc ''
        Hyprland package to use. Will override the 'xwayland' option.

        Defaults to the one provided by the flake. Set it to
        `pkgs.hyprland` to use the one provided by *nixpkgs* or
        if you have an overlay.

        Set to null to not add any Hyprland package to your path. This should
        be done if you want to use the NixOS module to install Hyprland.
      '';
    };

    ### INIT ###

    systemdIntegration = lib.mkOption {
      type = types.bool;
      default = pkgs.stdenv.isLinux;
      description = lib.mdDoc ''
        Whether to enable `hyprland-session.target` on
        hyprland startup. This links to `graphical-session.target`.
        Some important environment variables will be imported to systemd
        and dbus user environment before reaching the target, including:

        - DISPLAY
        - WAYLAND_DISPLAY
        - HYPRLAND_INSTANCE_SIGNATURE
        - XDG_CURRENT_DESKTOP
      '';
    };

    xwayland = {
      enable = lib.mkOption {
        type = types.bool;
        default = true;
        description = lib.mdDoc ''
          Enable XWayland.
        '';
      };
      hidpi = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc ''
          Enable HiDPI XWayland.
        '';
      };
    };

    nvidiaPatches = lib.mkOption {
      type = lib.types.bool;
      default = false;
      defaultText = lib.literalExpression "false";
      example = lib.literalExpression "true";
      description = lib.mdDoc ''
        Patch wlroots for better Nvidia support.
      '';
    };

    recommendedEnvironment = lib.mkOption {
      type = types.bool;
      default = true;
      description = lib.mdDoc ''
        Whether to set the recommended environment variables.
      '';
      # example = lib.literalExpression "";
    };

    extraInitConfig = lib.mkOption {
      type = types.nullOr types.lines;
      default = null;
      description = lib.mdDoc ''
        Extra configuration to be prepended to the top of
        `~/.config/hypr/hyprland.conf` (after module's generated init).
      '';
      # example = lib.literalExpression "";
    };

    ### CONFIG ###

    extraConfig = lib.mkOption {
      type = types.nullOr types.lines;
      default = null;
      description = lib.mdDoc ''
        Extra configuration lines to append to the bottom of
        `~/.config/hypr/hyprland.conf`.
      '';
      # example = lib.literalExpression "";
    };

    disableAutoreload = lib.mkOption {
      type = lib.types.bool;
      default = false;
      defaultText = lib.literalExpression "false";
      example = lib.literalExpression "true";
      description = lib.mdDoc ''
        Whether to disable automatically reloading Hyprland's configuration when
        rebuilding the Home Manager profile.
      '';
    };

    ### CONFIG: GENERAL ###

    config.general = {
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
        type = types.ints.unsigned;
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
        type = types.ints.unsigned;
        default = 5;
        description = lib.mdDoc ''
          Gap thickness between window edges, in pixels.

          *Renamed from: `gaps_in`*
        '';
        # example = lib.literalExpression "";
      };
      gaps_outside = lib.mkOption {
        type = types.ints.unsigned;
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
          The color of the border on an inactive window, in `0xAARRGGBB` format.

          *Renamed from: `col.inactive_border`*
        '';
        # example = lib.literalExpression "";
      };
      active_border_color = lib.mkOption {
        type = types.singleLineStr;
        default = "0xFFFFFFFF";
        description = lib.mdDoc ''
          The color of the border on an active window, in `0xAARRGGBB` format.

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

    ### CONFIG: DECORATION ###

    config.decoration = {
      rounding = lib.mkOption {
        type = types.ints.unsigned;
        default = 0;
        description = lib.mdDoc '''';
      };
      multisample_edges = lib.mkOption {
        type = types.bool;
        default = true;
        description = lib.mdDoc '''';
      };
      active_opacity = lib.mkOption {
        type = types.float;
        default = 1.0;
        description = lib.mdDoc '''';
      };
      inactive_opacity = lib.mkOption {
        type = types.float;
        default = 1.0;
        description = lib.mdDoc '''';
      };
      fullscreen_opacity = lib.mkOption {
        type = types.float;
        default = 1.0;
        description = lib.mdDoc '''';
      };
      blur = lib.mkOption {
        type = types.bool;
        default = true;
        description = lib.mdDoc '''';
      };
      blur_size = lib.mkOption {
        type = types.ints.positive;
        default = 8;
        description = lib.mdDoc '''';
      };
      blur_passes = lib.mkOption {
        type = types.ints.positive;
        default = 1;
        description = lib.mdDoc '''';
      };
      blur_ignore_opacity = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc '''';
      };
      blur_new_optimizations = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc '''';
      };
      drop_shadow = lib.mkOption {
        type = types.bool;
        default = true;
        description = lib.mdDoc '''';
      };
      shadow_range = lib.mkOption {
        type = types.ints.unsigned;
        default = 4;
        description = lib.mdDoc '''';
      };
      shadow_render_power = lib.mkOption {
        type = types.ints.unsigned;
        default = 3;
        description = lib.mdDoc '''';
      };
      shadow_ignore_window = lib.mkOption {
        type = types.bool;
        default = true;
        description = lib.mdDoc '''';
      };
      shadow_color = lib.mkOption {
        type = types.singleLineStr;
        # TODO colors
        default = "0xee1a1a1a";
        description = lib.mdDoc '''';
      };
      shadow_inactive_color = lib.mkOption {
        type = types.nullOr types.singleLineStr;
        default = null;
        description = lib.mdDoc '''';
      };
      shadow_offset = lib.mkOption {
        # TODO tuple
        type = types.listOf types.int;
        default = [0 0];
        description = lib.mdDoc '''';
      };
      dim_inactive = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc '''';
      };
      dim_strength = lib.mkOption {
        type = types.float;
        default = 0.5;
        description = lib.mdDoc '''';
      };
    };

    ### CONFIG: ANIMATIONS ###

    config.animations = {
      enable = lib.mkEnableOption "animations";

      animation = lib.mkOption {
        type = types.attrsOf (types.attrs);
        default = {};
        description = lib.mdDoc '''';
        example = lib.literalExpression ''
          {
            workspaces = {
              effect = "slide";
              enable = true;
              duration = 8;
              curve = "default";
            }
          }
        '';
      };

      bezier_curve = lib.mkOption {
        type = types.attrsOf (
          types.listOf (types.oneOf [types.float types.int])
        );
        # <https://easings.net>
        default = {
          linear = [0 0 1 1];

          easeInSine = [0.12 0 0.39 0];
          easeOutSine = [0.61 1 0.88 1];
          easeInOutSine = [0.37 0 0.63 1];

          easeInQuad = [0.11 0 0.5 0];
          easeOutQuad = [0.5 1 0.89 1];
          easeInOutQuad = [0.45 0 0.55 1];

          easeInCubic = [0.32 0 0.67 0];
          easeOutCubic = [0.33 1 0.68 1];
          easeInOutCubic = [0.65 0 0.35 1];

          easeInQuart = [0.5 0 0.75 0];
          easeOutQuart = [0.25 1 0.5 1];
          easeInOutQuart = [0.76 0 0.24 1];

          easeInQuint = [0.64 0 0.78 0];
          easeOutQuint = [0.22 1 0.36 1];
          easeInOutQuint = [0.83 0 0.17 1];

          easeInExpo = [0.7 0 0.84 0];
          easeOutExpo = [0.16 1 0.3 1];
          easeInOutExpo = [0.87 0 0.13 1];

          easeInCirc = [0.55 0 1 0.45];
          easeOutCirc = [0 0.55 0.45 1];
          easeInOutCirc = [0.85 0 0.15 1];

          easeInBack = [0.36 0 0.66 (-0.56)];
          easeOutBack = [0.34 1.56 0.64 1];
          easeInOutBack = [0.68 (-0.6) 0.32 1.6];
        };
        description = lib.mdDoc '''';
        # example = lib.literalExpression ''
        #   {
        #     easeInSine = [0.12 0 0.39 0];
        #     easeOutSine = [0.61 1 0.88 1];
        #     easeInOutSine = [0.37 0 0.63 1];

        #     easeInQuad = [0.11 0 0.5 0];
        #     easeOutQuad = [0.5 1 0.89 1];
        #     easeInOutQuad = [0.45 0 0.55 1];
        #   }
        # '';
      };
    };

    ### CONFIG: INPUT ###

    config.input = {
      keyboard = {
        layout = lib.mkOption {
          type = types.singleLineStr;
          default = "us";
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        variant = lib.mkOption {
          type = types.nullOr types.singleLineStr;
          default = null;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        model = lib.mkOption {
          type = types.nullOr types.singleLineStr;
          default = null;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        options = lib.mkOption {
          type = types.nullOr types.singleLineStr;
          default = null;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        rules = lib.mkOption {
          type = types.nullOr types.singleLineStr;
          default = null;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        file = lib.mkOption {
          type = types.nullOr types.singleLineStr;
          default = null;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
      };

      touchpad = {
        disable_while_typing = lib.mkOption {
          type = types.bool;
          default = true;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        natural_scroll = lib.mkOption {
          type = types.bool;
          default = false;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        clickfinger_behavior = lib.mkOption {
          type = types.bool;
          default = false;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        middle_button_emulation = lib.mkOption {
          type = types.bool;
          default = false;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        tap_to_click = lib.mkOption {
          type = types.bool;
          default = false;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        drag_lock = lib.mkOption {
          type = types.bool;
          default = false;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
        scroll_factor = lib.mkOption {
          type = types.float;
          default = 1.0;
          description = lib.mdDoc ''description'';
          example = lib.literalExpression '''';
        };
      };

      follow_mouse = lib.mkOption {
        type = types.enum [0 1 2 3];
        default = 1;
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      float_switch_override_focus = lib.mkOption {
        type = types.enum [0 1 2];
        default = 1;
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      repeat_rate = lib.mkOption {
        type = types.ints.positive;
        default = 25;
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      natural_scroll = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      numlock_by_default = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      force_no_accel = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      sensitivity = lib.mkOption {
        type = types.float;
        default = 0.0;
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      left_handed = lib.mkOption {
        type = types.bool;
        default = false;
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      accel_profile = lib.mkOption {
        # TODO why is the default `[EMPTY]`?
        type = types.enum ["adaptive" "flat"];
        default = "adaptive";
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
      scroll_method = lib.mkOption {
        # TODO why is the default `[EMPTY]`?
        type = types.enum ["2fg" "edge" "on_button_down" "no_scroll"];
        # type = types.nullOr (types.enum ["2fg" "edge" "on_button_down" "no_scroll"]);
        default = "2fg";
        description = lib.mdDoc ''description'';
        example = lib.literalExpression '''';
      };
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
        ### INIT ###

        ${(lib.optionalString cfg.systemdIntegration ''
          exec-once=${pkgs.dbus}/bin/dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP
          exec-once=systemctl --user start hyprland-session.target
        '')}

        ### EXTRA INIT ###

        ${cfg.extraInitConfig}

        ${(with cfg.config.general; ''
          ### GENERAL ###

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
        '')}

        ${(with cfg.config.decoration; ''
          ### DECORATION ###

          decoration {
            rounding = ${toString rounding}
            multisample_edges =  ${lib.boolToString multisample_edges}
            active_opacity = ${toString active_opacity}
            inactive_opacity = ${toString inactive_opacity}
            fullscreen_opacity = ${toString fullscreen_opacity}
            blur = ${lib.boolToString blur}
            blur_size = ${toString blur_size}
            blur_passes = ${toString blur_passes}
            blur_ignore_opacity = ${lib.boolToString blur_ignore_opacity}
            blur_new_optimizations = ${lib.boolToString blur_new_optimizations}
            drop_shadow = ${lib.boolToString drop_shadow}
            shadow_range = ${toString shadow_range}
            shadow_render_power = ${toString shadow_render_power}
            shadow_ignore_window = ${lib.boolToString shadow_ignore_window}
            col.shadow = ${shadow_color}
            col.shadow_inactive = ${shadow_inactive_color}
            shadow_offset = [${toString shadow_offset}]
            dim_inactive = ${lib.boolToString dim_inactive}
            dim_strength = ${toString dim_strength}
          }
        '')}

        ${(
          let
            msToDs = x: x / 100;
            bezierString = name: points: ''
              bezier = ${name}, ${
                lib.concatStringsSep "," (map toString points)
              }
            '';
            animationString = event: {
              enable ? true,
              duration,
              curve ? "default",
              style ? null,
            }: ''
              animation = ${event}, ${
                if enable
                then "1"
                else "0"
              }, ${
                toString (msToDs duration)
              }, ${curve}${lib.optionalString (style != null) ", ${style}"}
            '';
          in
            with cfg.config.animations; ''
              ### ANIMATIONS ###

              animations {
                enabled = ${lib.boolToString enable}

                ### BEZIER CURVES ###
                # <https://wiki.hyprland.org/Configuring/Animations/#curves>
                ${lib.concatStringsSep "  "
                (lib.mapAttrsToList bezierString bezier_curve)}

                ### ANIMATIONS ###
                # <https://wiki.hyprland.org/Configuring/Animations/>
                ${lib.concatStringsSep "  "
                (lib.mapAttrsToList animationString animation)}
              }
            ''
        )}

        ${(with cfg.config.input; ''
          ### INPUT ###

          input {
            kb_layout = ${toString keyboard.layout}
            kb_variant = ${toString keyboard.variant}
            kb_model = ${toString keyboard.model}
            kb_options = ${toString keyboard.options}
            kb_rules = ${toString keyboard.rules}
            kb_file = ${toString keyboard.file}
            follow_mouse = ${toString follow_mouse}
            float_switch_override_focus = ${toString float_switch_override_focus}
            repeat_rate = ${toString repeat_rate}
            natural_scroll = ${lib.boolToString natural_scroll}
            numlock_by_default = ${lib.boolToString numlock_by_default}
            force_no_accel = ${lib.boolToString force_no_accel}
            sensitivity = ${toString sensitivity}
            left_handed = ${lib.boolToString left_handed}
            accel_profile = ${toString accel_profile}
            scroll_method = ${toString scroll_method}

            touchpad {
              disable_while_typing = ${lib.boolToString touchpad.disable_while_typing}
              natural_scroll = ${lib.boolToString touchpad.natural_scroll}
              clickfinger_behavior = ${lib.boolToString touchpad.clickfinger_behavior}
              tap-to-click = ${lib.boolToString touchpad.tap_to_click}
              drag_lock = ${lib.boolToString touchpad.drag_lock}
              scroll_factor = ${toString touchpad.scroll_factor}
            }
          }
        '')}

        ${(lib.optionalString (cfg.extraConfig != null) ''
          ### EXTRA CONFIG ###

          ${cfg.extraConfig}
        '')}
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
        Description = cfg.package.meta.description;
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
