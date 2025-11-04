# Implementation Checklist

## Feature Parity Checklist

This document tracks the implementation of all Hyprland dwindle features to ensure 100% parity.

### Core Data Structures

- [ ] DwindleNode
  - [ ] Binary tree structure (parent, 2 children)
  - [ ] Window reference
  - [ ] Geometry (box)
  - [ ] Split configuration (splitTop, splitRatio)
  - [ ] Workspace tracking
  - [ ] Validity flag
  - [ ] Recursive recalculation method

- [ ] WindowElement
  - [ ] AXUIElement wrapper
  - [ ] Frame tracking
  - [ ] Floating state
  - [ ] Fullscreen state
  - [ ] Pseudotile state and size
  - [ ] Min/max size constraints
  - [ ] Workspace/monitor tracking

- [ ] Workspace
  - [ ] ID and name
  - [ ] Monitor assignment
  - [ ] Window collection
  - [ ] Fullscreen state tracking
  - [ ] Gap overrides

- [ ] Monitor
  - [ ] Display detection
  - [ ] Frame and visible frame
  - [ ] Reserved areas
  - [ ] Active workspace tracking
  - [ ] Multi-monitor support

### Configuration Options (dwindle:*)

- [ ] `pseudotile` - Enable pseudotiling mode
- [ ] `force_split` - Force split direction (0/1/2)
- [ ] `permanent_direction_override` - Keep direction after one window
- [ ] `preserve_split` - Remember manual split direction
- [ ] `special_scale_factor` - Scale for special workspaces
- [ ] `split_width_multiplier` - Width multiplier for split decisions
- [ ] `use_active_for_splits` - Use active window instead of cursor
- [ ] `default_split_ratio` - Default split ratio (0.1-1.9)
- [ ] `split_bias` - Bias toward new window
- [ ] `smart_split` - Cursor position relative split
- [ ] `smart_resizing` - Resize inner and outer nodes
- [ ] `precise_mouse_move` - Precise mouse movement
- [ ] `single_window_aspect_ratio` - Aspect ratio for single window
- [ ] `single_window_aspect_ratio_tolerance` - Tolerance for aspect ratio

### Gap Configuration

- [ ] `general:gaps_in` - Inner gaps (CSS format)
  - [ ] Top
  - [ ] Right
  - [ ] Bottom
  - [ ] Left

- [ ] `general:gaps_out` - Outer gaps (CSS format)
  - [ ] Top
  - [ ] Right
  - [ ] Bottom
  - [ ] Left

- [ ] Workspace-specific gap overrides
- [ ] Edge detection for gap application

### Window Lifecycle

- [ ] `onWindowCreatedTiling()`
  - [ ] Find opening target (cursor, active, first)
  - [ ] Handle direction override
  - [ ] Check size compatibility
  - [ ] First window fullscreen
  - [ ] Create parent node
  - [ ] Split logic
  - [ ] Size/position calculation

- [ ] `onWindowRemovedTiling()`
  - [ ] Exit fullscreen if needed
  - [ ] Get sibling node
  - [ ] Promote sibling
  - [ ] Update parent linkage
  - [ ] Mark invalid nodes
  - [ ] Recalculate layout

- [ ] `recalculateMonitor()`
  - [ ] Calculate all workspaces on monitor
  - [ ] Handle special workspaces

- [ ] `recalculateWindow()`
  - [ ] Recalculate specific window's node

### Split Logic Implementation

- [ ] Automatic split direction
  - [ ] Based on available size
  - [ ] Width multiplier consideration
  - [ ] Preserve split mode

- [ ] Cursor-based split
  - [ ] Detect cursor position in parent
  - [ ] Split based on which half

- [ ] Smart split
  - [ ] Calculate parent center
  - [ ] Calculate delta slope
  - [ ] Determine quadrant
  - [ ] Apply direction

- [ ] Force split
  - [ ] Always left/top (mode 1)
  - [ ] Always right/bottom (mode 2)

- [ ] Direction override
  - [ ] Up (vertical split, new on top)
  - [ ] Down (vertical split, new on bottom)
  - [ ] Left (horizontal split, new on left)
  - [ ] Right (horizontal split, new on right)
  - [ ] Permanent override option

- [ ] Split bias
  - [ ] Invert ratio when new window is first child

### Node Recalculation

- [ ] `recalcSizePosRecursive()`
  - [ ] Handle leaf nodes (apply to window)
  - [ ] Handle container nodes (split children)
  - [ ] Calculate available size with gaps
  - [ ] Determine split direction
  - [ ] Apply horizontal/vertical overrides
  - [ ] Calculate child boxes with gaps
  - [ ] Recurse to children

- [ ] Gap application
  - [ ] Edge detection
  - [ ] Inner gap calculation
  - [ ] Outer gap calculation
  - [ ] CSS-style gaps (per-edge)

### Window Operations

- [ ] `switchWindows(window1, window2)`
  - [ ] Store fullscreen modes
  - [ ] Exit fullscreen
  - [ ] Swap window references
  - [ ] Swap workspaces if needed
  - [ ] Recalculate layouts
  - [ ] Restore fullscreen

- [ ] `moveWindowTo(window, direction)`
  - [ ] Calculate focal point
  - [ ] Remove from current position
  - [ ] Set override focal point
  - [ ] Determine target monitor
  - [ ] Handle cross-monitor moves
  - [ ] Re-add at new position

- [ ] `alterSplitRatio(window, delta, exact)`
  - [ ] Get parent node
  - [ ] Calculate new ratio
  - [ ] Clamp to 0.1-1.9
  - [ ] Recalculate layout

### Resizing

- [ ] `resizeActiveWindow(pixResize, corner)`
  - [ ] Handle pseudotiling resize
  - [ ] Edge detection
  - [ ] Calculate allowed movement
  - [ ] Smart resizing vs basic

- [ ] Smart resizing
  - [ ] Identify inner/outer nodes
  - [ ] Handle corner parameter
  - [ ] Adjust multiple split ratios
  - [ ] Preserve window sizes

- [ ] Basic resizing
  - [ ] Find parent containers
  - [ ] Adjust single split ratio
  - [ ] Handle 1 or 2 axes of freedom

- [ ] Pseudotile resizing
  - [ ] Detect drag start
  - [ ] Determine extent flags
  - [ ] Resize pseudo size
  - [ ] Clamp to min/max
  - [ ] Apply to node

### Layout Messages

- [ ] `togglesplit`
  - [ ] Get parent node
  - [ ] Toggle splitTop flag
  - [ ] Recalculate

- [ ] `swapsplit`
  - [ ] Get parent node
  - [ ] Swap children[0] and children[1]
  - [ ] Recalculate

- [ ] `movetoroot [window] [stable|unstable]`
  - [ ] Find window (or use focused)
  - [ ] Walk to root
  - [ ] Track ancestry
  - [ ] Swap subtrees
  - [ ] Optionally swap children (stable)
  - [ ] Recalculate

- [ ] `preselect [direction]`
  - [ ] Parse direction (u/t/d/b/r/l)
  - [ ] Set override direction
  - [ ] Clear on other input

### Fullscreen Support

- [ ] `fullscreenRequestForWindow()`
  - [ ] FSMODE_NONE (restore)
    - [ ] Apply node data
    - [ ] Restore floating pos/size
  - [ ] FSMODE_FULLSCREEN
    - [ ] Set to monitor size
  - [ ] FSMODE_MAXIMIZED
    - [ ] Create fake node
    - [ ] Apply with gaps/reserved areas

### Special Features

- [ ] Pseudotiling
  - [ ] Center window in tile
  - [ ] Maintain fixed size
  - [ ] Scale down if needed
  - [ ] Custom resize handling

- [ ] Single window aspect ratio
  - [ ] Detect single window (no parent)
  - [ ] Calculate requested ratio
  - [ ] Apply padding
  - [ ] Respect tolerance

- [ ] Size constraints
  - [ ] Read min size from AX API
  - [ ] Read max size from AX API
  - [ ] Clamp window sizes
  - [ ] Center if smaller than tile

- [ ] Special workspace scaling
  - [ ] Detect special workspace
  - [ ] Apply scale factor
  - [ ] Center in tile

### Workspace Management

- [ ] Create workspace
- [ ] Switch workspace
- [ ] Move window to workspace
- [ ] Window visibility on inactive workspaces
- [ ] Per-workspace gaps
- [ ] Active workspace per monitor

### Monitor Management

- [ ] Detect displays
- [ ] Monitor add/remove events
- [ ] Calculate visible frame
- [ ] Reserved areas (menu bar, dock)
- [ ] Multi-monitor window movement
- [ ] Per-monitor workspaces

### Accessibility API Integration

- [ ] Permission checking
- [ ] Permission request UI
- [ ] Window enumeration
  - [ ] Get all apps
  - [ ] Get windows per app
  - [ ] Filter valid windows

- [ ] Window manipulation
  - [ ] Get frame (position + size)
  - [ ] Set frame (position + size)
  - [ ] Focus window
  - [ ] Get title
  - [ ] Get app name
  - [ ] Get min/max size

- [ ] Window observation
  - [ ] Window created notification
  - [ ] Window destroyed notification
  - [ ] Window moved notification
  - [ ] Window resized notification
  - [ ] Window focused notification
  - [ ] App launched notification
  - [ ] App terminated notification

### Configuration System

- [ ] TOML parser integration
- [ ] Config file structure
  - [ ] [general]
  - [ ] [dwindle]
  - [ ] [workspace_rules]
  - [ ] [window_rules]
  - [ ] [keybindings]

- [ ] Config loading
- [ ] Config saving
- [ ] Config hot-reload
- [ ] Default config generation

### Window Rules

- [ ] Match by app name
- [ ] Match by window title
- [ ] Match by window role
- [ ] Set floating
- [ ] Set pseudotile
- [ ] Set workspace
- [ ] Set monitor

### Hotkey System

- [ ] Carbon event registration
- [ ] Modifier key support
- [ ] Key code mapping
- [ ] Action binding
- [ ] Unregistration
- [ ] Config-based registration

- [ ] Navigation hotkeys
  - [ ] Focus left/right/up/down
  - [ ] Move window left/right/up/down

- [ ] Layout hotkeys
  - [ ] Toggle split
  - [ ] Swap split
  - [ ] Toggle floating
  - [ ] Toggle fullscreen

- [ ] Resize hotkeys
  - [ ] Increase split ratio
  - [ ] Decrease split ratio

- [ ] Workspace hotkeys
  - [ ] Switch to workspace N
  - [ ] Move window to workspace N

- [ ] Preselect hotkeys
  - [ ] Preselect up/down/left/right

### User Interface

- [ ] Menu bar app
  - [ ] Status item
  - [ ] Menu with actions
  - [ ] Enable/disable toggle

- [ ] Preferences window
  - [ ] General tab
  - [ ] Dwindle tab
  - [ ] Gaps tab
  - [ ] Keybindings tab
  - [ ] Window rules tab

- [ ] Permission request dialog
- [ ] First launch setup

### Testing

- [ ] Unit tests
  - [ ] Node tree operations
  - [ ] Split logic
  - [ ] Gap calculations
  - [ ] Recalculation algorithm
  - [ ] Window operations

- [ ] Integration tests
  - [ ] Full workflow
  - [ ] Multi-workspace
  - [ ] Multi-monitor
  - [ ] Config reload

- [ ] Manual testing checklist
  - [ ] All hotkeys work
  - [ ] All config options work
  - [ ] Window rules work
  - [ ] Multi-monitor works
  - [ ] Workspace switching works
  - [ ] All layout messages work

### Documentation

- [ ] README.md
- [ ] Installation guide
- [ ] Configuration guide
- [ ] Keybinding reference
- [ ] Troubleshooting guide
- [ ] Known limitations
- [ ] Contributing guide

### Performance

- [ ] Profile layout calculations
- [ ] Optimize hot paths
- [ ] Implement caching
- [ ] Batch recalculations
- [ ] Debounce rapid changes

### Polish

- [ ] Error handling
- [ ] Logging system
- [ ] Debug mode
- [ ] Crash recovery
- [ ] Version checking
- [ ] Auto-update (optional)

---

## Feature Parity Matrix

| Feature | Hyprland | macOS App | Notes |
|---------|----------|-----------|-------|
| Binary tree layout | ✓ | ⬜ | Core algorithm |
| Split direction auto | ✓ | ⬜ | Based on available space |
| Force split | ✓ | ⬜ | 3 modes |
| Smart split | ✓ | ⬜ | Cursor-relative |
| Preserve split | ✓ | ⬜ | Remember manual |
| Direction override | ✓ | ⬜ | Up/down/left/right |
| Split ratio | ✓ | ⬜ | 0.1-1.9 range |
| Split bias | ✓ | ⬜ | Favor new window |
| Smart resizing | ✓ | ⬜ | Multi-node resize |
| Pseudotiling | ✓ | ⬜ | Centered fixed size |
| Fullscreen | ✓ | ⬜ | NONE/FULL/MAXIMIZED |
| Gaps (inner/outer) | ✓ | ⬜ | CSS-style per-edge |
| Single window ratio | ✓ | ⬜ | Aspect ratio |
| Size constraints | ✓ | ⬜ | Min/max from app |
| Toggle split | ✓ | ⬜ | Layout message |
| Swap split | ✓ | ⬜ | Layout message |
| Move to root | ✓ | ⬜ | Layout message |
| Preselect | ✓ | ⬜ | Layout message |
| Switch windows | ✓ | ⬜ | Swap operation |
| Move window | ✓ | ⬜ | Directional move |
| Multi-workspace | ✓ | ⬜ | Virtual desktops |
| Multi-monitor | ✓ | ⬜ | Per-monitor workspaces |
| Window rules | ✓ | ⬜ | Per-app config |
| Workspace rules | ✓ | ⬜ | Per-workspace gaps |
| Animations | ✓ | ✗ | N/A (no compositor) |
| Window groups | ✓ | ✗ | Not core dwindle |
| Special workspace | ✓ | ⬜ | With scaling |

**Legend:**
- ✓ = Implemented in Hyprland
- ✗ = Not applicable/out of scope
- ⬜ = To be implemented in macOS app

---

## Testing Scenarios

### Basic Tiling

1. [ ] Open 1 window → takes full space
2. [ ] Open 2 windows → split in half
3. [ ] Open 3 windows → binary tree split
4. [ ] Close windows → siblings promoted correctly

### Split Modes

5. [ ] Cursor split → new window opens on cursor side
6. [ ] Force split left/top → always opens left/top
7. [ ] Force split right/bottom → always opens right/bottom
8. [ ] Smart split → opens in cursor quadrant
9. [ ] Preselect up → next window opens on top
10. [ ] Preselect down → next window opens on bottom
11. [ ] Preselect left → next window opens on left
12. [ ] Preselect right → next window opens on right

### Resizing

13. [ ] Resize window → split ratio changes
14. [ ] Resize with smart mode → multiple ratios change
15. [ ] Resize pseudotile → pseudo size changes
16. [ ] Resize at edge → no movement

### Layout Messages

17. [ ] Toggle split → switches horizontal/vertical
18. [ ] Swap split → swaps children positions
19. [ ] Move to root → maximizes window in layout
20. [ ] Move to root unstable → maximizes without swap

### Window Operations

21. [ ] Switch windows → swaps positions
22. [ ] Move window left → moves to left tile
23. [ ] Move window right → moves to right tile
24. [ ] Move window up → moves to top tile
25. [ ] Move window down → moves to bottom tile
26. [ ] Move across monitors → changes monitor

### Fullscreen

27. [ ] Toggle fullscreen → takes monitor size
28. [ ] Toggle maximize → respects gaps/bars
29. [ ] Exit fullscreen → returns to tile

### Pseudotiling

30. [ ] Enable pseudotile → centers in tile
31. [ ] Resize pseudotile → stays centered
32. [ ] Pseudotile too large → scales down

### Gaps

33. [ ] Set inner gaps → space between windows
34. [ ] Set outer gaps → space at edges
35. [ ] Per-workspace gaps → override works
36. [ ] Edge detection → correct gap application

### Workspaces

37. [ ] Switch workspace → windows hide/show
38. [ ] Move window to workspace → changes workspace
39. [ ] Per-workspace layout → independent trees

### Multi-Monitor

40. [ ] Windows on each monitor → separate layouts
41. [ ] Move window between monitors → transfers correctly
42. [ ] Different workspaces per monitor → works independently

### Configuration

43. [ ] Change config value → takes effect
44. [ ] Reload config → updates without restart
45. [ ] Invalid config → fallback to defaults

### Edge Cases

46. [ ] Very small window → respects min size
47. [ ] Very large window → respects max size
48. [ ] Single window → aspect ratio applied
49. [ ] Many windows → tree depth handled
50. [ ] Rapid window open/close → no crashes

---

## Progress Tracking

**Start Date**: _____________

**Phase 1 Complete**: _____________ (Foundation)
**Phase 2 Complete**: _____________ (Core Layout)
**Phase 3 Complete**: _____________ (Advanced Features)
**Phase 4 Complete**: _____________ (Workspace Management)
**Phase 5 Complete**: _____________ (Configuration)
**Phase 6 Complete**: _____________ (UI & Hotkeys)
**Phase 7 Complete**: _____________ (Layout Messages)
**Phase 8 Complete**: _____________ (Testing & Polish)

**Release Date**: _____________

---

## Notes

- This checklist covers 100% of Hyprland's dwindle layout features
- Some features are marked N/A (animations, groups) as they're not feasible on macOS without private APIs
- All core dwindle functionality has full parity
- Testing scenarios ensure all features work correctly
- Use this checklist to track implementation progress
