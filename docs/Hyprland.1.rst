========
Hyprland
========

---------------------------------
Dynamic tiling Wayland compositor
---------------------------------

:Date: 15 Jul 2022
:Copyright: Copyright (c) 2022, vaxerski
:Version: 0.7.1beta
:Manual section: 1
:Manual group: HYPRLAND

SYNOPSIS
========

``Hyprland`` [arg [...]].

DESCRIPTION
===========

``Hyprland`` is a dynamic tiling Wayland compositor based on
wlroots that doesn't sacrifice on its looks.

NOTICE
======

Hyprland is still in pretty early development compared to some other Wayland compositors.

Although Hyprland is pretty stable, it may have some bugs.

CONFIGURATION
=============

For configuration information please see <`https://github.com/hyprwm/Hyprland/wiki`>.

LAUNCHING
=========

You can launch Hyprland by either going into a TTY and executing ``Hyprland``, or with a login manager.

`IMPORTANT`: Do `not` launch ``Hyprland`` with `root` permissions (don't `sudo`)

Login managers are not officially supported, but here's a short compatibility list:

    * SDDM -> Works flawlessly.
    * GDM -> Works with the caveat of crashing `Hyprland` on the first launch.
    * ly -> Works with minor issues and/or caveats.

OPTIONS
=======

-h, --help
    Show command usage.

-c, --config
    Specify config file to use.

BUGS
====

Submit bug reports and request features online at:

    <`https://github.com/hyprwm/Hyprland/issues`>

SEE ALSO
========

Sources at: <`https://github.com/hyprwm/Hyprland`>

AUTHORS
=======

Vaxerski <`https://github.com/vaxerski`>
