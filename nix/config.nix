{
  lib,
  types,
  pkgs,
  cfg,
}: let
  millisToDecis = x: x / 100;
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
      toString (millisToDecis duration)
    }, ${curve}${lib.optionalString (style != null) ", ${style}"}
  '';
in ''
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

  ${(with cfg.config.animations; ''
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
  '')}

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

  ${(with cfg.config.gestures; ''
    ### GESTURES ###

    gestures {
      workspace_swipe = ${lib.boolToString workspace_swipe.enable}
      workspace_swipe_fingers = ${toString workspace_swipe.fingers}
      workspace_swipe_distance = ${toString workspace_swipe.distance}
      workspace_swipe_invert = ${lib.boolToString workspace_swipe.invert}
      workspace_swipe_min_speed_to_force = ${toString workspace_swipe.min_speed_to_force}
      workspace_swipe_cancel_ratio = ${toString workspace_swipe.cancel_ratio}
    }
  '')}

  ${(lib.optionalString (cfg.extraConfig != null) ''
    ### EXTRA CONFIG ###

    ${cfg.extraConfig}
  '')}
''
