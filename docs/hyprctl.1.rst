=======
hyprctl
=======

----------------------------------------------------------------
Utility for controlling parts of Hyprland from a CLI or a script
----------------------------------------------------------------

:Date: 15 Jul 2022
:Copyright: Copyright (c) 2022, vaxerski
:Version: 0.7.1beta
:Manual section: 1
:Manual group: hyprctl

SYNOPSIS
========

``hyprctl`` [(opt)flags] [command] [(opt)args]

DESCRIPTION
===========

``hyprctl`` is a utility for controlling some parts of the compositor from a CLI or a script.

COMMANDS
========

Control

    ``dispatch``

        Call a dispatcher with an argument.

        An argument must be present.
        For dispatchers without parameters it can be anything.

        Returns: `ok` on success, and an error message on failure.

        Examples:

            ``hyprctl`` `dispatch exec kitty`

            ``hyprctl`` `dispatch pseudo x`

    ``keyword``

        Set a config keyword dynamically.

        Returns: `ok` on success, and an error message on failure.

        Examples:

            ``hyprctl`` `keyword bind SUPER,0,pseudo`

            ``hyprctl`` `keyword general:border_size 10`

    ``reload``

        Force a reload of the config file.

    ``kill``

        Enter kill mode, where you can kill an app by clicking on it.
        You can exit by pressing ESCAPE.

Info

    ``version``

        Prints the Hyprland version, flags, commit and branch of build.

    ``monitors``

        Lists all the outputs with their properties.

    ``workspaces``

        Lists all workspaces with their properties.

    ``clients``

        Lists all windows with their properties.

    ``devices``

        Lists all connected input devices.

    ``activewindow``

        Returns the active window name.

    ``layers``

        Lists all the layers.

    ``splash``

        Returns the current random splash.

OPTIONS
=======

--batch
    Specify a batch of commands to execute.

    Example:

        ``hyprctl`` `--batch "keyword general:border_size 2 ; keyword general:gaps_out 20"`

        `;` separates the commands.

-j
    Outputs information in JSON.

BUGS
====

Submit bug reports and feature requests online at:

    <`https://github.com/hyprwm/hyprctl/issues`>

SEE ALSO
========

Sources at: <`https://github.com/hyprwm/hyprctl`>

AUTHORS
=======

Vaxerski  <`https://github.com/vaxerski`>
