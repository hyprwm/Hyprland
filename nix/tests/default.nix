inputs: pkgs: let
  flake = inputs.self.packages.${pkgs.stdenv.hostPlatform.system};
  hyprland = flake.hyprland;
in {
  tests = pkgs.testers.runNixOSTest {
    name = "hyprland-tests";

    nodes.machine = {pkgs, ...}: {
      environment.systemPackages = with pkgs; [
        flake.hyprtester

        # Programs needed for tests
        jq
        kitty
        xorg.xeyes
      ];

      # Enabled by default for some reason
      services.speechd.enable = false;

      environment.variables = {
        "AQ_TRACE" = "1";
        "HYPRLAND_TRACE" = "1";
        "XDG_RUNTIME_DIR" = "/tmp";
        "XDG_CACHE_HOME" = "/tmp";
        "KITTY_CONFIG_DIRECTORY" = "/etc/kitty/kitty.conf";
      };

      environment.etc."kitty/kitty.conf".text = ''
        confirm_os_window_close 0
      '';

      programs.hyprland = {
        enable = true;
        package = hyprland;
        # We don't need portals in this test, so we don't set portalPackage
      };

      # Test configuration
      environment.etc."test.conf".source = "${flake.hyprtester}/share/hypr/test.conf";

      # Disable portals
      xdg.portal.enable = pkgs.lib.mkForce false;

      # Autologin root into tty
      services.getty.autologinUser = "alice";

      system.stateVersion = "24.11";

      users.users.alice = {
        isNormalUser = true;
      };

      virtualisation = {
        cores = 4;
        # Might crash with less
        memorySize = 8192;
        resolution = {
          x = 1920;
          y = 1080;
        };

        # Doesn't seem to do much, thought it would fix XWayland crashing
        qemu.options = ["-vga none -device virtio-gpu-pci"];
      };
    };

    testScript = ''
      # Wait for tty to be up
      machine.wait_for_unit("multi-user.target")

      # Run hyprtester testing framework/suite
      print("Running hyprtester")
      exit_status, _out = machine.execute("su - alice -c 'hyprtester -b ${hyprland}/bin/Hyprland -c /etc/test.conf -p ${flake.hyprtester}/lib/hyprtestplugin.so 2>&1 | tee /tmp/testerlog; exit ''${PIPESTATUS[0]}'")
      print(f"Hyprtester exited with {exit_status}")

      # Copy logs to host
      machine.execute('cp "$(find /tmp/hypr -name *.log | head -1)" /tmp/hyprlog')
      machine.execute(f'echo {exit_status} > /tmp/exit_status')
      machine.copy_from_vm("/tmp/testerlog")
      machine.copy_from_vm("/tmp/hyprlog")
      machine.copy_from_vm("/tmp/exit_status")

      # Print logs for visibility in CI
      _, out = machine.execute("cat /tmp/testerlog")
      print(f"Hyprtester log:\n{out}")

      # Finally - shutdown
      machine.shutdown()
    '';
  };
}
