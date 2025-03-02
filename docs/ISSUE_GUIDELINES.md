# Issue Guidelines

First of all, please remember to:
- Check that your issue is not a duplicate
- Read the [FAQ](https://wiki.hyprland.org/FAQ/)
- Read the [Configuring Page](https://wiki.hyprland.org/Configuring/)

<br/>

# Reporting suggestions
Suggestions are welcome.

Many features can be implemented using bash scripts and Hyprland sockets, read up on those [Here](https://wiki.hyprland.org/IPC). Please do not suggest features that can be implemented as such.

<br/>

# Reporting bugs

All bug reports should have the following:
- Steps to reproduce
- Expected outcome
- Noted outcome

If your bug is one that doesn't crash Hyprland, but feels like invalid behavior, that's all you need to say.

If your bug crashes Hyprland, append additionally:
- The Hyprland log
- Your config
- (v0.22.0beta and up) The Hyprland Crash Report
- (v0.21.0beta and below) Coredump / Coredump analysis (with a stacktrace)

**Important**: Please do NOT use any package for reporting bugs! Clone and compile from source.

## Obtaining the Hyprland log
If you are in a TTY, and the hyprland session that crashed was the last one you launched, the log will be printed with
```
cat $XDG_RUNTIME_DIR/hypr/$(ls -t $XDG_RUNTIME_DIR/hypr | head -n 1)/hyprland.log
```
feel free to send it to a file, save, copy, etc.

if you are in a Hyprland session, and you want the log of the last session, use
```
cat $XDG_RUNTIME_DIR/hypr/$(ls -t $XDG_RUNTIME_DIR/hypr | head -n 2 | tail -n 1)/hyprland.log
```

basically, directories in $XDG_RUNTIME_DIR/hypr are your sessions.

## Obtaining the Hyprland Crash Report (v0.22.0beta and up)

If you have `$XDG_CACHE_HOME` set, the crash report directory is `$XDG_CACHE_HOME/hyprland`. If not, it's `$HOME/.cache/hyprland`.

Go to the crash report directory, where you should find a file named `hyprlandCrashReport[XXXX].txt` where `[XXXX]` is the PID of the process that crashed.

Attach that file to your issue.
## Obtaining the Hyprland coredump (v0.21.0beta and below)
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

## Obtaining the debug Hyprland coredump
A debug coredump provides more information for debugging and may speed up the process of fixing the bug.

Make sure you're on latest git. Run `git pull --recurse-submodules` to sync everything.

1. [Compile Hyprland with debug mode](http://wiki.hyprland.org/Contributing-and-Debugging/#build-in-debug-mode)
    > Note: The config file used will be `hyprlandd.conf` instead of `hyprland.conf`
2. `cd ~`
3. For your own convenience, launch Hyprland from a tty with the envvar `ASAN_OPTIONS="log_path=asan.log" ~/path/to/Hyprland`
4. Reproduce the crash. Hyprland should instantly close.
5. Check out your `~` and find a file called `asan.log.XXXXX` where `XXXXX` will be a number corresponding to the PID of the Hyprland instance that crashed.
6. That is your coredump. Attach it to your issue.
