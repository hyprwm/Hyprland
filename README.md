# BreakLand

**A Nix-first Wayland compositor.**

BreakLand is an independent Wayland compositor built from Hyprland, with a different philosophy: configuration should be declarative, common desktop tasks should be simple, and good defaults should not require hours of customization.

BreakLand is designed primarily for NixOS and Home Manager users.

## Philosophy

**1. Declarative by design.**

Configuration is defined through typed Nix modules rather than an imperative scripting language. The desktop should be reproducible, understandable, and easy to maintain.

**2. Good defaults are a feature.**

A clean, preconfigured Waybar, sensible keybindings, consistent animations, and practical window behavior should be available without downloading somebody else's dotfiles.

**3. Scrolling is a first-class workflow.**

Scrolling window layouts are central to BreakLand's design, not an afterthought. Window navigation should feel natural and predictable.

**4. Simple things should stay simple.**

Mirroring or extending displays should take a click. A wallpaper should automatically apply to every connected screen. Users shouldn't need to write scripts for ordinary desktop tasks.

**5. No unnecessary decoration.**

BreakLand ships without a default wallpaper and avoids unnecessary visual clutter. Appearance is customizable, but usability always comes first.

**6. Breaking compatibility is acceptable. Breaking functionality is not.**

BreakLand does not promise compatibility with Hyprland's configuration format, plugins, or internal APIs. Breaking changes must serve a clear technical or usability purpose.

**7. Independent and transparent.**

BreakLand follows its own technical roadmap. **Sponsorship must not grant exclusive control over development priorities.  ;-) ... for those who are in the know**

**8. Compatibility with other BreakLand versions**

Old BreakLand version will look the same way on the newest version. Old configs are fully reproducible.

## Origins

BreakLand is an independent fork of Hyprland, originally developed by the Hyprland contributors.

It is not affiliated with or endorsed by the Hyprland project.

BreakLand retains the required upstream copyright notices and applicable open-source licenses.
