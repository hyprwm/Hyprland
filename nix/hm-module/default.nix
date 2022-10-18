self: {
  config,
  lib,
  pkgs,
  ...
}: let
  inherit (lib) types;
  cfg = config.wayland.windowManager.hyprland;
  defaultHyprlandPackage = self.packages.${pkgs.system}.default.override {
    enableXWayland = cfg.xwayland.enable;
    hidpiXWayland = cfg.xwayland.hidpi;
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

    recommendedEnvironment = lib.mkOption {
      type = types.bool;
      default = true;
      description = lib.mdDoc ''
        Whether to set the recommended environment variables.
      '';
      # example = lib.literalExpression "";
    };

    xwayland.enable = lib.mkOption {
      type = types.bool;
      default = true;
      description = lib.mdDoc ''
        Enable XWayland.
      '';
    };

    xwayland.hidpi = lib.mkOption {
      type = types.bool;
      default = false;
      description = lib.mdDoc ''
        Enable HiDPI XWayland.
      '';
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

    ### CONFIG ###

    extraInitConfig = lib.mkOption {
      type = types.nullOr types.lines;
      default = null;
      description = lib.mdDoc ''
        Extra configuration to be prepended to the top of
        `~/.config/hypr/hyprland.conf` (after module's generated init).
      '';
      # example = lib.literalExpression "";
    };

    extraConfig = lib.mkOption {
      type = types.nullOr types.lines;
      default = null;
      description = lib.mdDoc ''
        Extra configuration lines to append to the bottom of
        `~/.config/hypr/hyprland.conf`.
      '';
    };

    config = builtins.mapAttrs (_: p: import p {inherit lib types;}) {
      general = ./config/general.nix;
      dwindle = ./config/dwindle.nix;
      input = ./config/input.nix;
      decoration = ./config/decoration.nix;
      gestures = ./config/gestures.nix;
      animations = ./config/animations.nix;
      misc = ./config/misc.nix;
      binds = ./config/binds.nix;
      debug = ./config/debug.nix;
      windowRules = ./config/windowrules.nix;
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
      text = import ./config.nix {inherit lib types pkgs cfg;};

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
