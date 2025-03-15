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
        "EGL_PLATFORM" = "wayland";
        "XWAYLAND_NO_GLAMOR" = "1";
        # Doesn't make a difference
        # "LIBGL_ALWAYS_SOFTWARE" = "1";
        "XDG_RUNTIME_DIR" = "/tmp";
        "XDG_CACHE_HOME" = "/tmp";
      };

      programs.hyprland = {
        enable = true;
        package = flake.hyprland;
        # We don't need portals in this test, so we don't set portalPackage
      };

      # Original conf
      environment.etc."test.conf".source = "${flake.hyprtester}/share/hypr/test.conf";
      # New conf without HEADLESS outputs, which don't work
      environment.etc."test2.conf".source = ./test.conf;

      # Disable portals
      xdg.portal.enable = pkgs.lib.mkForce false;

      # Autologin root into tty
      services.getty.autologinUser = "alice";

      system.stateVersion = "24.11";

      users.users.alice = {
        isNormalUser = true;
      };

      # Might crash with less
      virtualisation.memorySize = 8192;

      # Doesn't seem to do much, thought it would fix XWayland crashing
      virtualisation.qemu.options = ["-vga none -device virtio-gpu-pci"];
    };

    testScript = ''
      # Wait for tty to be up
      machine.wait_for_unit("multi-user.target")

      # Run hyprtester testing framework/suite
      machine.succeed("su - alice -c 'hyprtester -b ${flake.hyprland}/bin/Hyprland -c /etc/test2.conf -p ${flake.hyprtester}/lib/hyprtestplugin.so 2>&1 | tee /tmp/testerlog'")

      # Copy logs to host
      machine.execute('cp "$(find /tmp/hypr -name *.log | head -1)" /tmp/hyprlog')
      machine.copy_from_vm("/tmp/testerlog")
      machine.copy_from_vm("/tmp/hyprlog")

      # Print logs for visibility in CI
      _, out = machine.execute("cat /tmp/hyprlog")
      print(f"Hyprland log:\n{out}")
      _, out = machine.execute("cat /tmp/testerlog")
      print(f"Hyprtester log:\n{out}")

      # Finally - shutdown
      machine.shutdown()
    '';
  };
}
