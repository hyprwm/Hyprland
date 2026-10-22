# Per-corner rounding

The four optional decoration settings `rounding_top_left`, `rounding_top_right`,
`rounding_bottom_right`, and `rounding_bottom_left` override `rounding` for one
corner. Each defaults to `-1`, which inherits the window's effective `rounding`.
Zero keeps that corner square. Positive values use the same layout-pixel units
and perceived-roundness scaling as `rounding`.

The same names are available as dynamic window rule effects and through
`hl.dsp.window.set_prop()`. Returning a corner to `-1` restores inheritance. The
workspace rule `no_rounding` also applies to the individual corner properties,
with the existing property priority rules.

`rounding_power` still controls the shape of every rounded corner: `2` produces
circular arcs; `4` produces a superellipse with a fuller transition into the
straight edges. No new shape setting is needed.

For example, to keep the upper corners square:

```lua
hl.config({
    decoration = {
        rounding = 20,
        rounding_power = 4,
        rounding_top_left = 0,
        rounding_top_right = 0,
    },
})
```

This is useful for displays whose visible area has curved corners. A script or
plugin can round just the window corners touching those display corners while
keeping the corners between adjacent tiled windows square. Display detection,
hardware-specific radii, and the policy for when a window touches an edge remain
outside the compositor's renderer.

The implementation carries four radii through surfaces, borders, shadows, glow,
blur shapes, and closing-window snapshots. Input hit testing uses the same
corner selection and supports fractional rounding powers. For unequal radii,
arcs are bounded by half the smaller window dimension so adjacent quadrants do
not overlap.

Equal radii use the existing single-region rendering path. Unequal radii split
damaged regions into disjoint quadrants and reuse the existing shaders. This
adds draw calls for asymmetric corners, without adding fragment-shader
branches. Offset shadows partition the shadow and window cutout separately.
The extra draw calls are a performance tradeoff for asymmetric corners.

Local validation on 2026-10-22:

- Release build with GCC 16, tests enabled, XWayland and systemd disabled.
- All 791 unit tests pass, including seven geometry tests and two rule parsing
  tests added for this change.
- An isolated compositor session verifies the four IPC properties, global
  defaults, window rule priority, inheritance, and workspace `no_rounding`.
- Visual checks cover circles and superellipses, moving between a display
  corner and its interior, four different radii, offset shadows, inner glow,
  translucent Kawase blur, scale 1.25, a rotated output, and fullscreen.
- GPU elapsed-time queries on a continuously redrawn 2880x1800@120 scene
  compare uniform and selective radii in six randomized blocks per case.
  Median composition times are 2.555/2.548 ms for circles, 2.555/2.552 ms
  for superellipses, and 3.669/3.666 ms for superellipses with transparency,
  shadows, and glow. None of the paired mean-difference confidence intervals
  excludes zero; these measurements establish neither an overhead nor a
  speedup on this GPU. GPU clocks were not fixed and the host shared the GPU.
  This compares two configurations of the patched binary, not patched versus
  unpatched Hyprland. Nearly equal but unequal radii were also measured to
  exercise the quadrant split with minimal change in visible shape.
- Blur was inactive in those timed scenes because the terminal explicitly
  disabled it through the background-effect protocol. A separate visual check
  enabled the client's blur request and verified Kawase blur. Blur overhead
  and acrylic blur remain unvalidated.

The compositor ran on Mesa Intel Graphics (LNL) inside omabox, with a private
Aquamarine 0.15.1 build. Physical display geometry, XWayland, HDR, and multiple
outputs were not validated. The additional hyprtester property checks are
included for the project's integration runner; the local IPC checks were run
separately. Screenshots and recordings are kept outside the Git repository.
