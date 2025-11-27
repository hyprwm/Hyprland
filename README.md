<div align = center>

<img src="https://raw.githubusercontent.com/hyprwm/Hyprland/main/assets/header.svg" width="750" height="300" alt="banner">

<br>

[![Badge Workflow]][Workflow]
[![Badge License]][License] 
![Badge Language] 
[![Badge Pull Requests]][Pull Requests] 
[![Badge Issues]][Issues] 
![Badge Hi Mom]<br>

<br>

Hyprland is a 100% independent, dynamic tiling Wayland compositor that doesn't sacrifice on its looks.

It provides the latest Wayland features, is highly customizable, has all the eyecandy, the most powerful plugins,
easy IPC, much more QoL stuff than other compositors and more...
<br>
<br>

---

**[<kbd> <br> Install <br> </kbd>][Install]** 
**[<kbd> <br> Quick Start <br> </kbd>][Quick Start]** 
**[<kbd> <br> Configure <br> </kbd>][Configure]** 
**[<kbd> <br> Contribute <br> </kbd>][Contribute]**

---

<br>

</div>

# Features

- All of the eyecandy: gradient borders, blur, animations, shadows and much more
- A lot of customization
- 100% independent, no wlroots, no libweston, no kwin, no mutter.
- Custom bezier curves for the best animations
- Powerful plugin support
- Built-in plugin manager
- Tearing support for better gaming performance
- Easily expandable and readable codebase
- Fast and active development
- Not afraid to provide bleeding-edge features
- Config reloaded instantly upon saving
- Fully dynamic workspaces
- Two built-in layouts and more available as plugins
- Global keybinds passed to your apps of choice
- Tiling/pseudotiling/floating/fullscreen windows
- Special workspaces (scratchpads)
- Window groups (tabbed mode)
- Powerful window/monitor/layer rules
- Socket-based IPC
- Native IME and Input Panels Support
- and much more...

<br>

# System Requirements

## Minimum Requirements
- **OS**: Linux-based system with Wayland support
- **GPU**: Graphics card with OpenGL 3.0+ support
- **RAM**: 2GB minimum, 4GB recommended
- **CPU**: Modern x86_64 or ARM64 processor

## Dependencies
- **aquamarine** >= 0.9.0
- **hyprcursor** >= 0.1.7
- **hyprgraphics** >= 0.1.3
- **hyprlang** >= 0.3.2
- **hyprutils** >= 0.8.1
- **pixman**, **libdrm**, **cairo**, **pango**
- **wayland**, **wayland-protocols**
- **xkbcommon**, **libinput**
- **OpenGL**, **EGL**
- **(Optional)** XWayland for X11 app support

<br>

# Quick Build

```bash
# Clone the repository
git clone --recursive https://github.com/hyprwm/Hyprland
cd Hyprland

# Build with Meson (recommended)
meson setup build
ninja -C build

# Or build with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Install
sudo cmake --install build
# or
sudo ninja -C build install
```

For detailed installation instructions, see the [Installation Guide][Install].

<br>

# Project Structure

```
Hyprland/
├── src/              # Main source code
│   ├── config/       # Configuration handling
│   ├── desktop/      # Desktop shell components
│   ├── managers/     # Core managers (input, layout, etc.)
│   ├── protocols/    # Wayland protocol implementations
│   └── render/       # Rendering engine
├── hyprctl/          # Command-line control utility
├── hyprpm/           # Plugin manager
├── hyprtester/       # Testing framework
├── protocols/        # Wayland protocol definitions
└── docs/             # Documentation and man pages
```

<br>

# Development

## Building in Debug Mode
```bash
meson setup build --buildtype=debug
ninja -C build
```

## Running Tests
```bash
# Build and run the test suite
cmake -B build -DTESTS=ON
cmake --build build
ctest --test-dir build
```

## Code Style
This project uses:
- **C++26** standard
- **clang-format** for code formatting
- **clang-tidy** for static analysis

Run formatting before submitting:
```bash
clang-format -i src/**/*.{cpp,hpp}
```

<br>

# Community & Support

- 📖 **[Wiki](https://wiki.hypr.land)** - Comprehensive documentation
- 💬 **[Discord](https://discord.gg/hQ9XvMUjjr)** - Community chat
- 🐛 **[Issues][Issues]** - Bug reports and feature requests
- 🔧 **[Discussions](https://github.com/hyprwm/Hyprland/discussions)** - Questions and ideas

## Getting Help
1. Check the [FAQ](https://wiki.hypr.land/FAQ/)
2. Search [existing issues][Issues]
3. Read the [Configuration Guide][Configure]
4. Ask on Discord or GitHub Discussions

<br>

# Contributing

We welcome contributions! Please see:
- **[Contributing Guide][Contribute]** - How to contribute
- **[PR Guidelines](https://wiki.hypr.land/Contributing-and-Debugging/PR-Guidelines/)** - Pull request standards
- **[Issue Guidelines](docs/ISSUE_GUIDELINES.md)** - How to report bugs

**Before contributing:**
- Build from source (not from packages)
- Test your changes thoroughly
- Follow the code style guidelines
- Write clear commit messages

<br>

# License

Hyprland is licensed under the **BSD 3-Clause License**. See [LICENSE](LICENSE) for details.

<br>
<br>

<div align = center>

# Gallery

<br>

![Preview A]

<br>

![Preview B]

<br>

![Preview C]

<br>
<br>

</div>

# Special Thanks

<br>

**[wlroots]** - *For powering Hyprland in the past*

**[tinywl]** - *For showing how 2 do stuff*

**[Sway]** - *For showing how 2 do stuff the overkill way*

**[Vivarium]** - *For showing how 2 do stuff the simple way*

**[dwl]** - *For showing how 2 do stuff the hacky way*

**[Wayfire]** - *For showing how 2 do some graphics stuff*


<!----------------------------------------------------------------------------->

[Configure]: https://wiki.hypr.land/Configuring/
[Stars]: https://starchart.cc/hyprwm/Hyprland
[Hypr]: https://github.com/hyprwm/Hypr

[Pull Requests]: https://github.com/hyprwm/Hyprland/pulls
[Issues]: https://github.com/hyprwm/Hyprland/issues
[Todo]: https://github.com/hyprwm/Hyprland/projects?type=beta

[Contribute]: https://wiki.hypr.land/Contributing-and-Debugging/
[Install]: https://wiki.hypr.land/Getting-Started/Installation/
[Quick Start]: https://wiki.hypr.land/Getting-Started/Master-Tutorial/
[Workflow]: https://github.com/hyprwm/Hyprland/actions/workflows/ci.yaml
[License]: LICENSE


<!----------------------------------{ Thanks }--------------------------------->

[Vivarium]: https://github.com/inclement/vivarium
[WlRoots]: https://gitlab.freedesktop.org/wlroots/wlroots
[Wayfire]: https://github.com/WayfireWM/wayfire
[TinyWl]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/tinywl/tinywl.c
[Sway]: https://github.com/swaywm/sway
[DWL]: https://codeberg.org/dwl/dwl

<!----------------------------------{ Images }--------------------------------->

[Preview A]: https://i.ibb.co/XxFY75Mk/greerggergerhtrytghjnyhjn.png
[Preview B]: https://i.ibb.co/C1yTb0r/falf.png
[Preview C]: https://i.ibb.co/2Yc4q835/hyprland-preview-b.png


<!----------------------------------{ Badges }--------------------------------->

[Badge Workflow]: https://github.com/hyprwm/Hyprland/actions/workflows/ci.yaml/badge.svg

[Badge Issues]: https://img.shields.io/github/issues/hyprwm/Hyprland
[Badge Pull Requests]: https://img.shields.io/github/issues-pr/hyprwm/Hyprland
[Badge Language]: https://img.shields.io/github/languages/top/hyprwm/Hyprland
[Badge License]: https://img.shields.io/github/license/hyprwm/Hyprland
[Badge Lines]: https://img.shields.io/tokei/lines/github/hyprwm/Hyprland
[Badge Hi Mom]: https://img.shields.io/badge/Hi-mom!-ff69b4
