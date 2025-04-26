# Hyprland Development Security Policy

If you have a bug that affects the security of your system, you may
want to privately disclose it instead of making it immediately public.

## Supported versions

_Only_ the most recent release on Github is supported. There are no LTS releases.

## What is not a security issue

Some examples of issues that should not be reported as security issues:

- An app can execute a command when ran outside of a sandbox
- An app can write / read hyprland sockets when ran outside of a sandbox
- Crashes
- Things that are protected via permissions when the permission system is disabled

## What is a security issue

Some examples of issues that should be reported as security issues:

- Sandboxed application executing arbitrary code via Hyprland
- Application being able to modify Hyprland's code on the fly
- Application being able to keylog / track user's activity beyond what the wayland protocols allow

## How to report security issues

Please report your security issues via either of these channels:
- Mail: `vaxry [at] vaxry [dot] net`
- Matrix: `@vaxry:matrix.vaxry.net`
- Discord: `@vaxry`
