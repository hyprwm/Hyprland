:title: hyprctl(1)
:author: Vaxerski <*https://github.com/vaxerski*>

NAME
====

hyprctl - Utility for controlling parts of Hyprland from a CLI or a script

SYNOPSIS
========

**hyprctl** [*(opt)flags*] [**command**] [*(opt)args*]

DESCRIPTION
===========

**hyprctl** is a utility for controlling some parts of the compositor from a CLI or a script.

CONTROL COMMANDS
================

**dispatch**

    Call a dispatcher with an argument.

    An argument must be present.
    For dispatchers without parameters it can be anything.

    Returns: *ok* on success, and an error message on failure.

    Examples:
        **hyprctl** *dispatch exec kitty*

        **hyprctl** *dispatch pseudo x*

**keyword**

    Set a config keyword dynamically.

    Returns: *ok* on success, and an error message on failure.

    Examples:
        **hyprctl** *keyword bind SUPER,0,pseudo*

        **hyprctl** *keyword general:border_size 10*

**reload**

    Force a reload of the config file.

**kill**

    Enter kill mode, where you can kill an app by clicking on it.
    You can exit by pressing ESCAPE.

INFO COMMANDS
=============

**version**

    Prints the Hyprland version, flags, commit and branch of build.

**monitors**

    Lists all the outputs with their properties.

**workspaces**

    Lists all workspaces with their properties.

**clients**

    Lists all windows with their properties.

**devices**

    Lists all connected input devices.

**activewindow**

    Returns the active window name.

**layers**

    Lists all the layers.

**splash**

    Returns the current random splash.

OPTIONS
=======

**--batch**

    Specify a batch of commands to execute.

    Example:
        **hyprctl** *--batch "keyword general:border_size 2 ; keyword general:gaps_out 20"*

        *;* separates the commands.

**-j**

    Outputs information in JSON.

BUGS
====

Submit bug reports and request features online at:
    <*https://github.com/hyprwm/Hyprland/issues*>

SEE ALSO
========

Sources at: <*https://github.com/hyprwm/Hyprland*>

COPYRIGHT
=========

Copyright (c) 2022, vaxerski
