
![Badge Workflow]

<div align = center>

![Banner]

<br>

![Badge License]![Badge Lines]![Badge Language] <br>
![Badge Pull Requests]![Badge Issues]

<br>

[![Badge Discord]][Discord]

<br>
<br>

***A Dynamic Wayland Tiling Compositor***

<br>
<br>

</div>

---



For Hyprland without the land part, see [Hypr], the Xorg window manager.

Hyprland is in early dev, expect some bugs. However, once you get it working, it's pretty stable. :P

Hyprland needs testers! Try it out and report bugs or suggestions!

# Key features
 - Parabolic window animations
 - Config reloaded instantly upon saving
 - Easily expandable and readable codebase
 - Rounded corners
 - Window blur
 - Workspaces Protocol support
 - Damage tracking (experimental)
 - Fade in/out
 - Support for docks/whatever
 - Window rules
 - Monitor rules
 - Socket-based IPC
 - Tiling/floating/fullscreen windows
 - Moving/resizing windows

# Major to-dos
 - Input Methods (wlr_input_method_v2)
 - Animations (some new, like workspace)
 - Fix electron rendering issues
 - Optimization
 - Fix weird scroll on XWayland (if possible)
 - Become sane
 - STABILITY
 - More config options for tweakers
 - Improve hyprctl

# Installation
I do not maintain any packages, but some kind people have made them for me. If I missed any, please let me know.

**Warning:** since I am not the maintainer, I cannot guarantee that those packages will always work and be up to date. Use at your own disclosure. If they don't, try building manually.

_Arch (AUR, -git)_
```
yay -S hyprland-git
```
## Manual building
If your distro doesn't have Hyprland in its repositories, or you want to modify Hyprland,

please refer to the [Wiki Page][Install] for the installation instructions.
<br/>

# Configuring
Head onto the [Wiki Page][Configure] to see more.

Hyprland without a config is a bad idea!
<br/>

# Contributions
Very welcome! see [Contributing.md][Contribute] for instuctions and guidelines!
<br/>

<div align = center>

# Gallery
![Preview A]
![Preview B]

<br>

---

<br>

# Stars Over Time

[![Stars Preview]][Stars]

<br>

---

<br>

</div>

# Special Thanks
wlroots - for their amazing library

tinywl - for showing how 2 do stuff

sway - for showing how 2 do stuff the overkill way

vivarium - for showing how 2 do stuff the simple way

dwl - for showing how 2 do stuff the hacky way

wayfire - for showing how 2 do some graphics stuff


<!----------------------------------------------------------------------------->

[Contribute]: https://github.com/vaxerski/Hyprland/blob/main/CONTRIBUTING.md
[Configure]: https://github.com/vaxerski/Hyprland/wiki/Configuring-Hyprland
[Install]: https://github.com/vaxerski/Hyprland/wiki/Installation
[Discord]: https://discord.gg/hQ9XvMUjjr
[Stars]: https://starchart.cc/vaxerski/Hyprland
[Hypr]: https://github.com/vaxerski/Hypr


<!----------------------------------{ Images }--------------------------------->

[Stars Preview]: https://starchart.cc/vaxerski/Hyprland.svg
[Preview A]: https://i.imgur.com/ZA4Fa8R.png
[Preview B]: https://i.imgur.com/BpXxM8H.png
[Banner]: https://raw.githubusercontent.com/vaxerski/Hyprland/main/assets/hyprland.png


<!----------------------------------{ Badges }--------------------------------->

[Badge Workflow]: https://github.com/vaxerski/Hyprland/actions/workflows/ci.yaml/badge.svg

[Badge Discord]: https://img.shields.io/badge/Discord-7289DA?style=for-the-badge&logo=discord&logoColor=white
[Badge Issues]: https://img.shields.io/github/issues/vaxerski/Hyprland?style=for-the-badge&label=%ef%bc%a9%ef%bd%93%ef%bd%93%ef%bd%95%ef%bd%85%ef%bd%93
[Badge Pull Requests]: https://img.shields.io/github/issues-pr/vaxerski/Hyprland?style=for-the-badge&label=%ef%bc%b0%ef%bd%95%ef%bd%8c%ef%bd%8c%20%ef%bc%b2%ef%bd%85%ef%bd%91%ef%bd%95%ef%bd%85%ef%bd%93%ef%bd%94%ef%bd%93
[Badge Language]: https://img.shields.io/github/languages/top/vaxerski/Hyprland?style=for-the-badge&label=%ef%bc%a3%ef%bc%8b%ef%bc%8b
[Badge License]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg?style=for-the-badge&label=%ef%bc%ac%ef%bd%89%ef%bd%83%ef%bd%85%ef%bd%8e%ef%bd%93%ef%bd%85
[Badge Lines]: https://img.shields.io/tokei/lines/github/vaxerski/Hyprland?style=for-the-badge&label=%ef%bc%a3%ef%bd%8f%ef%bd%84%ef%bd%85
