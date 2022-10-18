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
      repeat_delay = ${toString repeat_delay}
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

  # https://regex101.com/r/qvXGZJ/1
  # https://regex101.com/r/ZYB9sk/1

  ${(with cfg.config.misc; ''
    # <https://wiki.hyprland.org/Configuring/Variables/#misc>
    misc {
      disable_hyprland_logo = ${lib.boolToString disable_hyprland_logo}
      disable_splash_rendering = ${lib.boolToString disable_splash_rendering}
      no_vfr = ${lib.boolToString no_vfr}
      damage_entire_on_snapshot = ${lib.boolToString damage_entire_on_snapshot}
      mouse_move_enables_dpms = ${lib.boolToString mouse_move_enables_dpms}
      always_follow_on_dnd = ${lib.boolToString always_follow_on_dnd}
      layers_hog_keyboard_focus = ${lib.boolToString layers_hog_keyboard_focus}
      animate_manual_resizes = ${lib.boolToString animate_manual_resizes}
      disable_autoreload = ${lib.boolToString disable_autoreload}
      enable_swallow = ${lib.boolToString enable_swallow}
      swallow_regex = ${toString swallow_regex}
    }
  '')}

  ${(with cfg.config.binds; ''
    # <https://wiki.hyprland.org/Configuring/Variables/#binds>
    binds {
      pass_mouse_when_bound = ${lib.boolToString pass_mouse_when_bound}
      scroll_event_delay = ${toString scroll_event_delay}
      workspace_back_and_forth = ${lib.boolToString workspace_back_and_forth}
      allow_workspace_cycles = ${lib.boolToString allow_workspace_cycles}
    }
  '')}


  ${(with cfg.config.dwindle; ''
    # <https://wiki.hyprland.org/Configuring/Dwindle-Layout/>
    dwindle {
      pseudotile = ${lib.boolToString pseudotile}
      col.group_border = ${toString group_border_color}
      col.group_border_active = ${toString group_border_active_color}
      force_split = ${toString force_split}
      preserve_split = ${lib.boolToString preserve_split}
      special_scale_factor = ${toString special_scale_factor}
      split_width_multiplier = ${toString split_width_multiplier}
      no_gaps_when_only = ${lib.boolToString no_gaps_when_only}
      use_active_for_splits = ${lib.boolToString use_active_for_splits}
    }
  '')}

  ${(with cfg.config.debug; ''
    # <https://wiki.hyprland.org/Configuring/Variables/#debug>
    debug {
      overlay = ${lib.boolToString overlay}
      damage_blink = ${lib.boolToString damage_blink}
      disable_logs = ${lib.boolToString disable_logs}
      disable_time = ${lib.boolToString disable_time}
    }
  '')}

  ### EXTRA CONFIG ###

  ${toString cfg.extraConfig}
''
