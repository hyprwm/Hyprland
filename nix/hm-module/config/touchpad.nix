{
  lib,
  types,
}: {
  disable_while_typing = lib.mkOption {
    type = types.bool;
    default = true;
    description = lib.mdDoc ''
      The most generic form of "palm recognition", disable touch
      events while processing keyboard activity, continue after a timeout.

      <https://wayland.freedesktop.org/libinput/doc/latest/configuration.html#disable-while-typing>
    '';
    # example = lib.literalExpression '''';
  };
  natural_scroll = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Whether to reverse scrolling to "natural" direction.

      <https://wayland.freedesktop.org/libinput/doc/latest/configuration.html#scrolling>
    '';
    example = lib.literalExpression '''';
  };
  clickfinger_behavior = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Read: <https://wayland.freedesktop.org/libinput/doc/latest/clickpad-softbuttons.html#clickfinger-behavior>

      **Enabled**, `true`:

      > This is the default behavior on Apple touchpads.
      > Here, a left, right, middle button event is generated when one, two,
      > or three fingers are held down on the touchpad when a physical click is generated.
      > The location of the fingers does not matter and there are no software-defined button areas.
      >
      > [One, two and three-finger click with Clickfinger behavior](https://wayland.freedesktop.org/libinput/doc/latest/_images/clickfinger.svg)


      **Disabled**, `false`:

      > On some touchpads, libinput imposes a limit on how the fingers may
      > be placed on the touchpad. In the most common use-case this allows for a
      > user to trigger a click with the thumb while leaving the pointer-moving finger on the touchpad.
      >
      > [Illustration of the distance detection algorithm](https://wayland.freedesktop.org/libinput/doc/latest/_images/clickfinger-distance.svg)
    '';
    # example = lib.literalExpression '''';
  };
  middle_button_emulation = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Read: <https://wayland.freedesktop.org/libinput/doc/latest/middle-button-emulation.html#middle-button-emulation>

      **Enabled**, `true`:

      > When middle button emulation is enabled, a simultaneous press of the left
      > and right button generates a middle mouse button event. Releasing the buttons
      > generates a middle mouse button release, the left and right button events are discarded otherwise.

      **Disabled**, `false`:

      Use the default behavior of the touchpad device (physical button or otherwise).

      > Some devices provide middle mouse button emulation but do not allow enabling/disabling that emulation.
      > Likewise, some devices may allow middle button emulation but have it disabled by default.
      > This is the case for most mouse-like devices where a middle button is detected.
    '';
    # example = lib.literalExpression '''';
  };
  tap_to_click = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      Emulate mouse left, middle, and right clicks when tapping with one, two, or three fingers.

      <https://wayland.freedesktop.org/libinput/doc/latest/configuration.html#tap-to-click>
    '';
    # example = lib.literalExpression '''';
  };
  drag_lock = lib.mkOption {
    type = types.bool;
    default = false;
    description = lib.mdDoc ''
      A quick tap and then holding fingers down while moving emulates a held primary button press,
      allowing dragging. Drag lock adds a timeout before finishing the drag  when a finger is lifted.

      <https://wayland.freedesktop.org/libinput/doc/latest/tapping.html#tap-and-drag>
    '';
    # example = lib.literalExpression '''';
  };
  scroll_factor = lib.mkOption {
    type = types.float;
    default = 1.0;
    # TODO verify, link unhelpful
    description = lib.mdDoc ''
      Ratio of screen space movement to input events.

      <https://wayland.freedesktop.org/libinput/doc/latest/scrolling.html#scrolling>
    '';
    # example = lib.literalExpression '''';
  };
}
