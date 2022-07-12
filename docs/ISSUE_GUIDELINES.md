# Issue Guidelines

First of all, please remember to:
- Check that your issue is not a duplicate
- Read the [FAQ](https://github.com/vaxerski/Hyprland/wiki/FAQ)
- Read the [Configuring Page](https://github.com/vaxerski/Hyprland/wiki/Configuring-Hyprland)

<br/>

# Reporting suggestions
Suggestions are welcome.

Many features can be implemented using bash scripts and Hyprland sockets, read up on those [Here](https://github.com/vaxerski/Hyprland/wiki/IPC). Please do not suggest features that can be implemented as such.

<br/>

# Reporting bugs

All bug reports should have the following:
- Steps to reproduce
- Expected outcome
- Noted outcome

If your bug is one that doesn't crash Hyprland, but feels like invalid behavior, that's all you need to say.

If your bug crashes Hyprland, append additionally:
- The Hyprland log
- Coredump / Coredump analysis (with a stacktrace)
- Your config

**Important**: Please do NOT use any package for reporting bugs! Clone and compile from source.

## Obtaining the Hyprland log
If you are in a TTY, and the hyprland session that crashed was the last one you launched, the log will be printed with
```
cat /tmp/hypr/$(ls -t /tmp/hypr/ | head -n 1)/hyprland.log
```
feel free to send it to a file, save, copy, etc.

if you are in a Hyprland session, and you want the log of the last session, use
```
cat /tmp/hypr/$(ls -t /tmp/hypr/ | head -n 2 | tail -n 1)/hyprland.log
```

basically, directories in /tmp/hypr are your sessions.

## Obtaining the Hyprland coredump
If you are on systemd, you can simply use
```
coredumpctl
```
then go to the end (press END on your keyboard) and remember the PID of the last `Hyprland` occurrence. It's the first number after the time, for example `2891`.

exit coredumpctl (ctrl+c) and use
```
coredumpctl info [PID]
```
where `[PID]` is the PID you remembered.

