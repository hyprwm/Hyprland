inputs: pkgs: {
  xeyes = pkgs.testers.runNixOSTest {
    name = "kitty-xeyes";

    nodes.machine = {pkgs, ...}: {
      # Install kitty and xeyes
      environment.systemPackages = with pkgs; [
        kitty
        xorg.xeyes
        gdb
      ];

      # Automatically configure and start Sway when logging in on tty1:
      programs.bash.loginShellInit = ''
        if [ "$(tty)" = "/dev/tty1" ]; then
          set -e

          # sed s/Mod4/Mod1/ /etc/sway/config > ~/.config/sway/config

          # Hyprland --validate
          Hyprland && touch /tmp/hyprland-exit-ok
        fi
      '';

      programs.hyprland = {
        enable = true;
        package = inputs.self.packages.${pkgs.stdenv.hostPlatform.system}.hyprland-debug;
        portalPackage = inputs.self.packages.${pkgs.stdenv.hostPlatform.system}.xdg-desktop-portal-hyprland;
      };

      environment.etc."xdg/hypr/hyprland.conf".source = ./test.conf;
      environment.etc."xdg/hypr/hyprlandd.conf".source = ./test.conf;

      environment.variables = {
        "AQ_TRACE" = "1";
        "HYPRLAND_TRACE" = "1";
        "LIBGL_ALWAYS_SOFTWARE" = "1";
        "XDG_RUNTIME_DIR" = "/tmp";
        "XDG_CACHE_HOME" = "/tmp";
      };

      users.users.alice = {
        isNormalUser = true;
        extraGroups = ["wheel"];
        initialPassword = "123";
      };

      system.stateVersion = "24.11";

      # Autologin alice into tty
      services.getty.autologinUser = "alice";

      # Cannot start with `-vga std`
      # virtualisation.qemu.options = ["-vga none -device virtio-gpu-pci"];
      virtualisation.memorySize = 2048;
    };

    # > Running type check (enable/disable: config.skipTypeCheck)
    # > See https://nixos.org/manual/nixos/stable/#test-opt-skipTypeCheck
    # > testScriptWithTypes:74: error: Unpacking a string is disallowed  [misc]
    # >             _, ret = ret
    # >                      ^~~
    # > testScriptWithTypes:79: error: Argument 1 to "loads" has incompatible type
    # > "str | tuple[int, str]"; expected "str | bytes | bytearray"  [arg-type]
    # >         parsed = json.loads(ret)
    # >                             ^~~
    # > Found 2 errors in 1 file (checked 1 source file)
    skipTypeCheck = true;

    testScript = ''
      import shlex
      # import json

      q = shlex.quote


      def hyprctl(command: str = "", succeed: bool = True):
          assert command != "", "Must specify command"
          shell = q(f"hyprctl -i 0 {q(command)}")
          with machine.nested(
              f"sending hyprctl {shell!r}" + " (allowed to fail)" * (not succeed)
          ):
              ret = (machine.succeed if succeed else machine.execute)(
                  f"su -- alice -c {shell}"
              )

          # execute also returns a status code, but disregard.
          if not succeed:
              _, ret = ret

          if not succeed and not ret:
              return None

          # parsed = json.loads(ret)
          # return parsed
          return ret


      machine.wait_for_unit("multi-user.target")
      # Wait for Hyprland to start
      # machine.wait_for_unit("graphical-session.target", "alice", 5)
      machine.wait_for_file("/tmp/wayland-1")
      machine.execute("sleep 5")
      # Launch 2 kitties
      hyprctl('dispatch exec kitty')
      hyprctl('dispatch exec kitty')
      # Launch xeyes
      hyprctl('dispatch exec xeyes')
      # Wait until all 3 windows have appeared on workspace 1
      machine.wait_until_succeeds("su -- alice -c '[ $(hyprctl -i 0 clients | grep \"workspace: 1\" | wc -l) -eq 3 ]'")
      # Check xeyes is floating
      machine.succeed("su -- alice -c 'hyprctl -i 0 clients | grep \"floating: 1\" '")
      # Set xeyes to tiling
      machine.succeed("su -- alice -c 'hyprctl -i 0 dispatch settiled class:XEyes'")
      # Check xeyes is tiling
      machine.fail("su -- alice -c 'hyprctl -i 0 clients | grep \"floating: 1\"'")

      # Finally - shutdown
      machine.shutdown()
    '';
  };
}
