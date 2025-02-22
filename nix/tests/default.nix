inputs: pkgs: let
  flake = inputs.self.packages.${pkgs.stdenv.hostPlatform.system};
in {
  tests = pkgs.testers.runNixOSTest {
    name = "hyprland-tests";

    nodes.machine = {pkgs, ...}: {
      environment.systemPackages = with pkgs; [
        flake.hyprtester
        gdb

        # Programs needed for tests
        kitty
        xorg.xeyes
      ];

      # Enabled by default for some reason
      services.speechd.enable = false;

      environment.variables = {
        "AQ_TRACE" = "1";
        "HYPRLAND_TRACE" = "1";
        # Doesn't make a difference
        # "LIBGL_ALWAYS_SOFTWARE" = "1";
        "XDG_RUNTIME_DIR" = "/tmp";
        "XDG_CACHE_HOME" = "/tmp";
      };

      # Automatically configure and start hyprtester when logging in on tty1:
      # programs.bash.loginShellInit = ''
      #   if [ "$(tty)" = "/dev/tty1" ] && [ ! -f /tmp/shell-nologin ]; then
      #     set -e
      #     # prevent this if from running again
      #     touch /tmp/shell-nologin
      # 
      #     hyprtester -b ${flake.hyprland-debug}/bin/Hyprland -c ${flake.hyprtester}/share/hypr/test2.conf; touch /tmp/hyprland-exit
      #   fi
      # '';

      programs.hyprland = {
        enable = true;
        package = flake.hyprland-debug;
        # We don't need portals in this test, so we don't set portalPackage
      };

      # Original conf
      environment.etc."test.conf".source = "${flake.hyprtester}/share/hypr/test.conf";
      # New conf without HEADLESS outputs, which don't work
      environment.etc."test2.conf".source = ./test.conf;

      # Disable portals
      xdg.portal.enable = pkgs.lib.mkForce false;

      # Autologin root into tty
      services.getty.autologinUser = "root";

      system.stateVersion = "24.11";

      # Might crash with less
      virtualisation.memorySize = 8192;

      # Doesn't seem to do much, thought it would fix XWayland crashing
      virtualisation.qemu.options = [ "-vga none -device virtio-gpu-pci" ];
    };

    testScript = ''
      # Wait for tty to be up
      machine.wait_for_unit("multi-user.target")

      # Run hyprtester testing framework/suite
      machine.execute("hyprtester -b ${flake.hyprland-debug}/bin/Hyprland -c /etc/test2.conf")

      # Finally - shutdown
      machine.shutdown()
    '';
  };
}
