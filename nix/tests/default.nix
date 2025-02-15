inputs: pkgs: let
  flake = inputs.self.packages.${pkgs.stdenv.hostPlatform.system};
in {
  tests = pkgs.testers.runNixOSTest {
    name = "hyprland-tests";

    nodes.machine = {pkgs, ...}: {
      environment.systemPackages = with pkgs; [
        flake.hyprtester

        # Programs needed for tests
        kitty
        xorg.xeyes
      ];

      # Enabled by default for some reason
      services.speechd.enable = false;

      environment.variables = {
        "AQ_TRACE" = "1";
        "HYPRLAND_TRACE" = "1";
        "LIBGL_ALWAYS_SOFTWARE" = "1";
        "XDG_RUNTIME_DIR" = "/tmp";
        "XDG_CACHE_HOME" = "/tmp";
      };

      # Automatically configure and start Hyprland when logging in on tty1:
      programs.bash.loginShellInit = ''
        if [ "$(tty)" = "/dev/tty1" ] && [ ! -f /tmp/hyprland-exit ]; then
          set -e

          # hyprtester -b ${flake.hyprland}/bin/Hyprland -c ${flake.hyprtester}/share/hypr/test.conf; touch /tmp/hyprland-exit
          hyprtester -b ${flake.hyprland}/bin/Hyprland -c /etc/test.conf; touch /tmp/hyprland-exit
        fi
      '';

      programs.hyprland = {
        enable = true;
        package = flake.hyprland;
        # We don't need portals in this test, so we don't set portalPackage
      };

      environment.etc."test.conf".source = ./test.conf;

      # Disable portals
      xdg.portal.enable = pkgs.lib.mkForce false;

      users.users.alice = {
        isNormalUser = true;
        extraGroups = ["wheel"];
        initialPassword = "123";
      };

      # Autologin alice into tty
      services.getty.autologinUser = "alice";

      system.stateVersion = "24.11";

      # Might crash with less
      virtualisation.memorySize = 2048;
    };

    testScript = ''
      # Wait for tty to be up
      machine.wait_for_unit("multi-user.target")

      # Run hyprtester testing framework/suite
      # machine.succeed("su -- alice -c 'hyprtester -b ${flake.hyprland}/bin/Hyprland -c /etc/test.conf'")
      # 
      machine.wait_for_file("/tmp/hyprland-exit")

      # Finally - shutdown
      machine.shutdown()
    '';
  };
}
