inputs: pkgs:
let
  flake = inputs.self.packages.${pkgs.stdenv.hostPlatform.system};
  breakland = flake.breakland-with-tests;
in
{
  tests = pkgs.testers.runNixOSTest {
    name = "breakland-tests";

    nodes.machine =
      { pkgs, ... }:
      {
        imports = [ inputs.self.nixosModules.default ];

        environment.systemPackages = with pkgs; [
          # Programs needed for tests
          jq
          kitty
          wl-clipboard
          xeyes
        ];

        # Enabled by default for some reason
        services.speechd.enable = false;

        environment.variables = {
          "AQ_TRACE" = "1";
          "HYPRLAND_TRACE" = "1";
          "XDG_RUNTIME_DIR" = "/tmp";
          "XDG_CACHE_HOME" = "/tmp";
          "KITTY_CONFIG_DIRECTORY" = "/etc/kitty";
        };

        environment.etc."kitty/kitty.conf".text = ''
          confirm_os_window_close 0
          remember_window_size no
          initial_window_width  640
          initial_window_height 400
        '';

         programs.breakland = {
          enable = true;
           package = breakland;
          # We don't need portals in this test, so we don't set portalPackage
        };

        # Test configuration
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
          qemu.options = [ "-vga none -device virtio-gpu-pci" ];
        };
      };

    testScript = ''
      # Wait for tty to be up
      machine.wait_for_unit("multi-user.target")


      # Run gtests
      print("Running gtests")
       exit_status, _out = machine.execute("su - alice -c 'breakland_gtests 2>&1 | tee /tmp/gtestslog; exit ''${PIPESTATUS[0]}'")
      machine.execute(f'echo {exit_status} > /tmp/exit_status_gtests')

      # Print logs for visibility in CI
      _, out = machine.execute("cat /tmp/gtestslog")
      print(f"gtests log:\n{out}")

       # Copy compositor logs to the host
       machine.execute('cp "$(find /tmp/hypr -name *.log | head -1)" /tmp/hyprlog')
       machine.execute('cp /tmp/exit_status_gtests /tmp/exit_status')
       machine.copy_from_machine("/tmp/gtestslog")
       machine.copy_from_machine("/tmp/hyprlog")
       machine.copy_from_machine("/tmp/exit_status_gtests")

      # Finally - shutdown
      machine.shutdown()
    '';
  };
}
