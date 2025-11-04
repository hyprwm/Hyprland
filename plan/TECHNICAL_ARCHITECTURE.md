# macOS Dwindle Window Manager - Technical Architecture Plan

## Executive Summary

This document outlines the technical architecture for a Swift-based macOS application that implements Hyprland's dwindle tiling layout algorithm with 100% feature parity. The application will use macOS Accessibility APIs exclusively to avoid requiring SIP (System Integrity Protection) to be disabled.

**Key Constraints:**
- Swift + SwiftUI for implementation
- macOS Accessibility API only (no private APIs)
- No compositor control (no custom animations)
- No SIP disabling required
- Full parity with Hyprland dwindle layout features

---

## 1. Architecture Overview

### 1.1 Application Structure

```
DwindleWindowManager/
├── Core/
│   ├── WindowManagement/      # AX API window control
│   ├── LayoutEngine/          # Dwindle algorithm implementation
│   ├── TreeStructure/         # Binary tree data structures
│   └── EventSystem/           # Window event handling
├── Configuration/
│   ├── ConfigManager/         # Config file parsing & management
│   ├── WorkspaceRules/        # Per-workspace configuration
│   └── WindowRules/           # Per-window/app rules
├── Accessibility/
│   ├── AXBridge/              # Accessibility API wrapper
│   ├── WindowObserver/        # Window lifecycle monitoring
│   └── PermissionManager/     # AX permission handling
├── UI/
│   ├── MenuBar/               # Status bar integration
│   ├── PreferencesWindow/     # Settings UI
│   └── HotKeyManager/         # Global keyboard shortcuts
└── Utilities/
    ├── GeometryHelpers/       # CGRect/CGPoint utilities
    ├── MonitorManager/        # Display detection & management
    └── WorkspaceManager/      # Virtual workspace management
```

### 1.2 Technology Stack

- **Language**: Swift 5.9+
- **UI Framework**: SwiftUI + AppKit
- **APIs**:
  - Accessibility API (AXUIElement)
  - Core Graphics (CGWindow, CGDisplay)
  - IOKit (for display info)
  - Carbon (for global hotkeys)
- **Build System**: Swift Package Manager
- **Testing**: XCTest + UI Testing
- **Config Format**: TOML (similar to Hyprland)

---

## 2. Core Data Structures

### 2.1 Node Tree Structure

```swift
/// Represents a node in the dwindle binary tree
class DwindleNode {
    // Identity
    var id: UUID
    var workspaceID: UUID
    weak var parent: DwindleNode?
    var isNode: Bool  // true = container, false = window

    // Tree structure
    var children: [DwindleNode?] = [nil, nil]  // Max 2 children

    // Window reference (if leaf node)
    var window: WindowElement?

    // Geometry
    var box: CGRect

    // Split configuration
    var splitTop: Bool = false  // false = horizontal, true = vertical
    var splitRatio: Float = 1.0  // Range: 0.1 - 1.9

    // State
    var valid: Bool = true
    var ignoreFullscreenChecks: Bool = false

    // Methods
    func recalcSizePosRecursive(force: Bool = false,
                               horizontalOverride: Bool = false,
                               verticalOverride: Bool = false)
}
```

### 2.2 Window Element Wrapper

```swift
/// Wraps AXUIElement with additional metadata
class WindowElement {
    let axElement: AXUIElement
    let pid: pid_t
    let windowID: CGWindowID

    // Window state
    var frame: CGRect
    var isFloating: Bool = false
    var isFullscreen: Bool = false
    var isPseudotiled: Bool = false
    var pseudoSize: CGSize = .zero

    // Window properties
    var title: String
    var appName: String
    var windowRole: String

    // Workspace tracking
    var workspaceID: UUID
    var monitorID: UUID

    // Size constraints
    var minSize: CGSize?
    var maxSize: CGSize?

    // Methods
    func setFrame(_ frame: CGRect, animate: Bool = false)
    func focus()
    func minimize()
    func close()
    func bringToFront()
}
```

### 2.3 Workspace Data

```swift
struct Workspace {
    let id: UUID
    var name: String
    var monitorID: UUID

    // Layout state
    var hasFullscreenWindow: Bool = false
    var fullscreenMode: FullscreenMode = .none
    var fullscreenWindow: WindowElement?

    // Gap configuration (can override global)
    var gapsIn: EdgeInsets?
    var gapsOut: EdgeInsets?

    // Windows on this workspace
    var windows: Set<UUID>  // Window IDs

    func getFirstWindow() -> WindowElement?
    func getRootNode() -> DwindleNode?
}

struct EdgeInsets {
    var top: CGFloat
    var right: CGFloat
    var bottom: CGFloat
    var left: CGFloat
}
```

### 2.4 Monitor Configuration

```swift
struct Monitor {
    let id: UUID
    let displayID: CGDirectDisplayID

    var frame: CGRect
    var visibleFrame: CGRect  // Excluding menu bar

    // Reserved areas (menu bar, dock, etc.)
    var reservedTopLeft: CGPoint
    var reservedBottomRight: CGPoint

    var activeWorkspaceID: UUID
    var workspaces: [UUID]

    var name: String
    var isMain: Bool
}
```

---

## 3. Dwindle Configuration (100% Parity)

### 3.1 Configuration Structure

```swift
struct DwindleConfig {
    // Core split behavior
    var pseudotile: Bool = false
    var forceSplit: ForceSplitMode = .cursor
    var permanentDirectionOverride: Bool = false
    var preserveSplit: Bool = false

    // Split parameters
    var splitWidthMultiplier: Float = 1.0
    var useActiveForSplits: Bool = true
    var defaultSplitRatio: Float = 1.0  // 0.1 - 1.9
    var splitBias: Bool = false

    // Smart features
    var smartSplit: Bool = false
    var smartResizing: Bool = true
    var preciseMouseMove: Bool = false

    // Special workspace
    var specialScaleFactor: Float = 1.0

    // Aspect ratio (single window)
    var singleWindowAspectRatio: CGSize = .zero  // 0 = disabled
    var singleWindowAspectRatioTolerance: Float = 0.1
}

enum ForceSplitMode: Int {
    case cursor = 0      // Based on cursor position
    case leftTop = 1     // Always left/top
    case rightBottom = 2 // Always right/bottom
}

enum Direction: Int {
    case `default` = -1
    case up = 0
    case right = 1
    case down = 2
    case left = 3
}
```

### 3.2 Gap Configuration

```swift
struct GapsConfig {
    var `in`: EdgeInsets = EdgeInsets(top: 5, right: 5, bottom: 5, left: 5)
    var out: EdgeInsets = EdgeInsets(top: 10, right: 10, bottom: 10, left: 10)
}

struct WorkspaceGaps {
    var `in`: EdgeInsets
    var out: EdgeInsets
}
```

---

## 4. Accessibility API Integration

### 4.1 AXBridge Implementation

```swift
class AXBridge {
    static let shared = AXBridge()

    // Permission management
    func checkAccessibility() -> Bool {
        return AXIsProcessTrusted()
    }

    func requestAccessibility() {
        let options = [kAXTrustedCheckOptionPrompt.takeUnretainedValue(): true] as CFDictionary
        AXIsProcessTrustedWithOptions(options)
    }

    // Window enumeration
    func getAllWindows() -> [WindowElement] {
        var windows: [WindowElement] = []

        // Get all running applications
        let apps = NSWorkspace.shared.runningApplications

        for app in apps {
            guard let axApp = AXUIElementCreateApplication(app.processIdentifier) as AXUIElement? else {
                continue
            }

            var windowsValue: CFTypeRef?
            let result = AXUIElementCopyAttributeValue(axApp, kAXWindowsAttribute as CFString, &windowsValue)

            if result == .success, let axWindows = windowsValue as? [AXUIElement] {
                for axWindow in axWindows {
                    if let window = WindowElement(axElement: axWindow, pid: app.processIdentifier) {
                        windows.append(window)
                    }
                }
            }
        }

        return windows
    }

    // Window manipulation
    func setWindowFrame(_ window: AXUIElement, frame: CGRect) -> Bool {
        // Set position
        var position = CGPoint(x: frame.origin.x, y: frame.origin.y)
        let positionValue = AXValueCreate(.cgPoint, &position)!
        let posResult = AXUIElementSetAttributeValue(window, kAXPositionAttribute as CFString, positionValue)

        // Set size
        var size = CGSize(width: frame.size.width, height: frame.size.height)
        let sizeValue = AXValueCreate(.cgSize, &size)!
        let sizeResult = AXUIElementSetAttributeValue(window, kAXSizeAttribute as CFString, sizeValue)

        return posResult == .success && sizeResult == .success
    }

    func getWindowFrame(_ window: AXUIElement) -> CGRect? {
        var positionValue: CFTypeRef?
        var sizeValue: CFTypeRef?

        guard AXUIElementCopyAttributeValue(window, kAXPositionAttribute as CFString, &positionValue) == .success,
              AXUIElementCopyAttributeValue(window, kAXSizeAttribute as CFString, &sizeValue) == .success else {
            return nil
        }

        var point = CGPoint.zero
        var size = CGSize.zero

        AXValueGetValue(positionValue as! AXValue, .cgPoint, &point)
        AXValueGetValue(sizeValue as! AXValue, .cgSize, &size)

        return CGRect(origin: point, size: size)
    }

    func focusWindow(_ window: AXUIElement) -> Bool {
        let result = AXUIElementPerformAction(window, kAXRaiseAction as CFString)
        return result == .success
    }

    func getWindowTitle(_ window: AXUIElement) -> String? {
        var titleValue: CFTypeRef?
        guard AXUIElementCopyAttributeValue(window, kAXTitleAttribute as CFString, &titleValue) == .success else {
            return nil
        }
        return titleValue as? String
    }

    // Get min/max size constraints
    func getWindowMinSize(_ window: AXUIElement) -> CGSize? {
        var value: CFTypeRef?
        guard AXUIElementCopyAttributeValue(window, kAXMinSizeAttribute as CFString, &value) == .success else {
            return nil
        }
        var size = CGSize.zero
        AXValueGetValue(value as! AXValue, .cgSize, &size)
        return size
    }

    func getWindowMaxSize(_ window: AXUIElement) -> CGSize? {
        var value: CFTypeRef?
        guard AXUIElementCopyAttributeValue(window, kAXMaxSizeAttribute as CFString, &value) == .success else {
            return nil
        }
        var size = CGSize.zero
        AXValueGetValue(value as! AXValue, .cgSize, &size)
        return size
    }
}
```

### 4.2 Window Observer

```swift
class WindowObserver {
    private var observers: [pid_t: AXObserver] = [:]
    weak var delegate: WindowObserverDelegate?

    func startObserving() {
        // Monitor NSWorkspace for app launches
        NSWorkspace.shared.notificationCenter.addObserver(
            self,
            selector: #selector(appLaunched),
            name: NSWorkspace.didLaunchApplicationNotification,
            object: nil
        )

        NSWorkspace.shared.notificationCenter.addObserver(
            self,
            selector: #selector(appTerminated),
            name: NSWorkspace.didTerminateApplicationNotification,
            object: nil
        )

        // Observe existing applications
        for app in NSWorkspace.shared.runningApplications {
            observeApp(app)
        }
    }

    private func observeApp(_ app: NSRunningApplication) {
        guard let axApp = AXUIElementCreateApplication(app.processIdentifier) as? AXUIElement else {
            return
        }

        var observer: AXObserver?
        guard AXObserverCreate(app.processIdentifier, observerCallback, &observer) == .success,
              let observer = observer else {
            return
        }

        // Add notifications we care about
        let notifications = [
            kAXWindowCreatedNotification,
            kAXUIElementDestroyedNotification,
            kAXWindowMovedNotification,
            kAXWindowResizedNotification,
            kAXWindowMiniaturizedNotification,
            kAXWindowDeminiaturizedNotification,
            kAXFocusedWindowChangedNotification
        ]

        for notification in notifications {
            AXObserverAddNotification(observer, axApp, notification as CFString, nil)
        }

        CFRunLoopAddSource(
            RunLoop.current.getCFRunLoop(),
            AXObserverGetRunLoopSource(observer),
            .defaultMode
        )

        observers[app.processIdentifier] = observer
    }

    @objc private func appLaunched(_ notification: Notification) {
        guard let app = notification.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication else {
            return
        }
        observeApp(app)
    }

    @objc private func appTerminated(_ notification: Notification) {
        guard let app = notification.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication else {
            return
        }
        observers.removeValue(forKey: app.processIdentifier)
    }
}

// Callback for AX notifications
private func observerCallback(
    observer: AXObserver,
    element: AXUIElement,
    notification: CFString,
    refcon: UnsafeMutableRawPointer?
) {
    let notificationName = notification as String

    // Forward to WindowObserver delegate
    DispatchQueue.main.async {
        switch notificationName {
        case kAXWindowCreatedNotification as String:
            NotificationCenter.default.post(name: .windowCreated, object: element)
        case kAXUIElementDestroyedNotification as String:
            NotificationCenter.default.post(name: .windowDestroyed, object: element)
        case kAXWindowMovedNotification as String:
            NotificationCenter.default.post(name: .windowMoved, object: element)
        case kAXWindowResizedNotification as String:
            NotificationCenter.default.post(name: .windowResized, object: element)
        case kAXFocusedWindowChangedNotification as String:
            NotificationCenter.default.post(name: .windowFocused, object: element)
        default:
            break
        }
    }
}

protocol WindowObserverDelegate: AnyObject {
    func windowCreated(_ window: AXUIElement)
    func windowDestroyed(_ window: AXUIElement)
    func windowMoved(_ window: AXUIElement)
    func windowResized(_ window: AXUIElement)
    func windowFocused(_ window: AXUIElement)
}
```

---

## 5. Layout Engine Implementation

### 5.1 DwindleLayout Core

```swift
class DwindleLayout {
    // Configuration
    var config: DwindleConfig
    var gapsConfig: GapsConfig

    // Node storage
    private var nodes: [DwindleNode] = []

    // Override state
    private var overrideDirection: Direction = .default
    private var overrideFocalPoint: CGPoint?

    // Pseudo drag state
    private struct PseudoDragFlags {
        var started: Bool = false
        var pseudo: Bool = false
        var xExtent: Bool = false
        var yExtent: Bool = false
    }
    private var pseudoDragFlags = PseudoDragFlags()

    // MARK: - Core Operations

    /// Called when a new window is created
    func onWindowCreatedTiling(_ window: WindowElement, direction: Direction = .default) {
        guard !window.isFloating else { return }

        // Create new node for window
        let newNode = DwindleNode(window: window)
        nodes.append(newNode)

        // Handle direction override
        if direction != .default && overrideDirection == .default {
            overrideDirection = direction
        }

        // Find where to open the window
        let openingOn = findOpeningNode(for: window)

        // Check if window size is compatible
        if let openingOn = openingOn,
           !isWindowSizeCompatible(window, with: openingOn) {
            // Make it floating instead
            window.isFloating = true
            nodes.removeLast()
            return
        }

        // If first window on workspace, make it fullscreen
        guard let openingOn = openingOn,
              openingOn.window?.id != window.id else {
            let monitor = getMonitor(for: window)
            newNode.box = calculateFullscreenBox(for: monitor)
            applyNodeDataToWindow(newNode)
            return
        }

        // Create parent node and split
        createParentNodeAndSplit(newNode: newNode, openingOn: openingOn)
    }

    /// Called when a window is removed
    func onWindowRemovedTiling(_ window: WindowElement) {
        guard let node = getNode(for: window) else { return }

        // Handle fullscreen
        if window.isFullscreen {
            // Exit fullscreen
            window.isFullscreen = false
        }

        guard let parent = node.parent else {
            // Last window, just remove
            nodes.removeAll { $0.id == node.id }
            return
        }

        // Get sibling
        let sibling = parent.children[0]?.id == node.id ?
            parent.children[1] : parent.children[0]

        guard let sibling = sibling else { return }

        // Promote sibling to parent's position
        sibling.box = parent.box
        sibling.parent = parent.parent

        if let grandparent = parent.parent {
            if grandparent.children[0]?.id == parent.id {
                grandparent.children[0] = sibling
            } else {
                grandparent.children[1] = sibling
            }
        }

        // Mark as invalid and remove
        parent.valid = false
        node.valid = false

        nodes.removeAll { !$0.valid }

        // Recalculate layout
        if let siblingParent = sibling.parent {
            siblingParent.recalcSizePosRecursive()
        } else {
            sibling.recalcSizePosRecursive()
        }
    }

    /// Recalculates entire monitor layout
    func recalculateMonitor(_ monitorID: UUID) {
        guard let monitor = MonitorManager.shared.getMonitor(monitorID) else { return }

        for workspace in monitor.workspaces {
            calculateWorkspace(workspace)
        }
    }

    /// Recalculates a specific window
    func recalculateWindow(_ window: WindowElement) {
        guard let node = getNode(for: window) else { return }
        node.recalcSizePosRecursive()
    }

    // MARK: - Split Logic

    private func createParentNodeAndSplit(newNode: DwindleNode, openingOn: DwindleNode) {
        let parent = DwindleNode()
        parent.box = openingOn.box
        parent.workspaceID = openingOn.workspaceID
        parent.parent = openingOn.parent
        parent.isNode = true
        parent.splitRatio = config.defaultSplitRatio.clamped(to: 0.1...1.9)

        nodes.append(parent)

        // Determine split direction
        let monitor = getMonitor(for: newNode.window!)
        let edges = getNodeDisplayEdgeFlags(parent.box, monitor: monitor)
        let workspace = WorkspaceManager.shared.getWorkspace(parent.workspaceID)
        let gaps = getWorkspaceGaps(workspace)

        let availableSize = calculateAvailableSize(
            box: parent.box,
            edges: edges,
            gapsIn: gaps.in,
            gapsOut: gaps.out
        )

        let sideBySide = availableSize.width > availableSize.height * CGFloat(config.splitWidthMultiplier)
        parent.splitTop = !sideBySide

        // Apply overrides and smart split
        var horizontalOverride = false
        var verticalOverride = false

        if overrideDirection != .default {
            applyDirectionOverride(parent: parent, newNode: newNode, openingOn: openingOn,
                                 horizontalOverride: &horizontalOverride,
                                 verticalOverride: &verticalOverride)
        } else if config.smartSplit {
            applySmartSplit(parent: parent, newNode: newNode, openingOn: openingOn)
        } else if config.forceSplit != .cursor {
            applyForceSplit(parent: parent, newNode: newNode, openingOn: openingOn)
        } else {
            applyCursorSplit(parent: parent, newNode: newNode, openingOn: openingOn,
                           sideBySide: sideBySide)
        }

        // Apply split bias
        if config.splitBias && parent.children[0]?.id == newNode.id {
            parent.splitRatio = 2.0 - parent.splitRatio
        }

        // Update parent linkage
        if let grandparent = openingOn.parent {
            if grandparent.children[0]?.id == openingOn.id {
                grandparent.children[0] = parent
            } else {
                grandparent.children[1] = parent
            }
        }

        // Update children
        openingOn.parent = parent
        newNode.parent = parent

        // Calculate initial boxes
        let splitSide = !parent.splitTop
        if splitSide {
            // Horizontal split
            openingOn.box = CGRect(
                origin: parent.box.origin,
                size: CGSize(width: parent.box.width / 2, height: parent.box.height)
            )
            newNode.box = CGRect(
                origin: CGPoint(x: parent.box.origin.x + parent.box.width / 2, y: parent.box.origin.y),
                size: CGSize(width: parent.box.width / 2, height: parent.box.height)
            )
        } else {
            // Vertical split
            openingOn.box = CGRect(
                origin: parent.box.origin,
                size: CGSize(width: parent.box.width, height: parent.box.height / 2)
            )
            newNode.box = CGRect(
                origin: CGPoint(x: parent.box.origin.x, y: parent.box.origin.y + parent.box.height / 2),
                size: CGSize(width: parent.box.width, height: parent.box.height / 2)
            )
        }

        // Recalculate with proper gaps
        parent.recalcSizePosRecursive(horizontalOverride: horizontalOverride,
                                     verticalOverride: verticalOverride)
    }

    // MARK: - Window Operations

    func switchWindows(_ window1: WindowElement, _ window2: WindowElement) {
        guard let node1 = getNode(for: window1),
              let node2 = getNode(for: window2) else { return }

        let mode1 = window1.fullscreenMode
        let mode2 = window2.fullscreenMode

        // Exit fullscreen temporarily
        window1.isFullscreen = false
        window2.isFullscreen = false

        // Swap windows
        node1.window = window2
        node2.window = window1

        // If different workspaces, swap those too
        if node1.workspaceID != node2.workspaceID {
            swap(&window1.monitorID, &window2.monitorID)
            swap(&window1.workspaceID, &window2.workspaceID)
        }

        // Recalculate
        getMasterNode(workspaceID: node1.workspaceID)?.recalcSizePosRecursive()
        if node1.workspaceID != node2.workspaceID {
            getMasterNode(workspaceID: node2.workspaceID)?.recalcSizePosRecursive()
        }

        // Restore fullscreen
        window1.fullscreenMode = mode2
        window2.fullscreenMode = mode1
    }

    func moveWindowTo(_ window: WindowElement, direction: String) {
        guard let node = getNode(for: window) else { return }

        let originalWorkspaceID = window.workspaceID
        let originalPos = window.frame.center

        // Calculate focal point in direction
        let focalPoint = calculateFocalPoint(from: window.frame, direction: direction)

        // Remove from current position
        onWindowRemovedTiling(window)

        // Set override focal point
        overrideFocalPoint = focalPoint

        // Determine target monitor
        let targetMonitor = MonitorManager.shared.getMonitor(at: focalPoint)
        if targetMonitor.id != window.monitorID {
            window.monitorID = targetMonitor.id
            window.workspaceID = targetMonitor.activeWorkspaceID
        }

        // Re-add at new position
        onWindowCreatedTiling(window)

        overrideFocalPoint = nil
    }

    func alterSplitRatio(_ window: WindowElement, delta: Float, exact: Bool = false) {
        guard let node = getNode(for: window),
              let parent = node.parent else { return }

        let newRatio = exact ? delta : parent.splitRatio + delta
        parent.splitRatio = newRatio.clamped(to: 0.1...1.9)

        parent.recalcSizePosRecursive()
    }

    // MARK: - Resizing

    func resizeActiveWindow(_ pixResize: CGSize, corner: RectCorner = .none, window: WindowElement?) {
        guard let window = window ?? getActiveWindow(),
              let node = getNode(for: window) else { return }

        // Handle pseudotiling
        if window.isPseudotiled {
            resizePseudotiled(node: node, pixResize: pixResize)
            return
        }

        let monitor = getMonitor(for: window)
        let edges = getNodeDisplayEdgeFlags(node.box, monitor: monitor)

        // Calculate allowed movement
        var allowedMovement = pixResize
        if edges.left && edges.right {
            allowedMovement.width = 0
        }
        if edges.top && edges.bottom {
            allowedMovement.height = 0
        }

        if config.smartResizing {
            resizeWithSmartResizing(node: node, allowedMovement: allowedMovement, corner: corner)
        } else {
            resizeBasic(node: node, allowedMovement: allowedMovement)
        }
    }

    private func resizeWithSmartResizing(node: DwindleNode, allowedMovement: CGSize, corner: RectCorner) {
        // Implementation of smart resizing algorithm from Hyprland
        // Resizes both inner and outer nodes for better UX
        // See DwindleLayout.cpp:731-788
    }

    // MARK: - Layout Messages

    func layoutMessage(_ message: String, window: WindowElement?) -> Any? {
        let args = message.split(separator: " ").map(String.init)

        guard let command = args.first else { return nil }

        switch command {
        case "togglesplit":
            toggleSplit(window)
        case "swapsplit":
            swapSplit(window)
        case "movetoroot":
            let targetWindow = args.count > 1 ?
                findWindow(matching: args[1]) : window
            let stable = args.count > 2 ? args[2] != "unstable" : true
            moveToRoot(targetWindow, stable: stable)
        case "preselect":
            guard args.count > 1 else { return nil }
            preselectDirection(args[1])
        default:
            break
        }

        return nil
    }

    func toggleSplit(_ window: WindowElement?) {
        guard let window = window,
              let node = getNode(for: window),
              let parent = node.parent,
              !window.isFullscreen else { return }

        parent.splitTop = !parent.splitTop
        parent.recalcSizePosRecursive()
    }

    func swapSplit(_ window: WindowElement?) {
        guard let window = window,
              let node = getNode(for: window),
              let parent = node.parent,
              !window.isFullscreen else { return }

        swap(&parent.children[0], &parent.children[1])
        parent.recalcSizePosRecursive()
    }

    func moveToRoot(_ window: WindowElement?, stable: Bool = true) {
        guard let window = window,
              let node = getNode(for: window),
              !window.isFullscreen,
              let parent = node.parent,
              parent.parent != nil else { return }

        // Find path to root
        var ancestor = node
        var root = parent

        while let nextParent = root.parent {
            ancestor = root
            root = nextParent
        }

        // Swap with other subtree
        let nodeToSwap = node.parent!.children[0]?.id == node.id ?
            node.parent!.children[0] : node.parent!.children[1]
        let otherSubtree = root.children[0]?.id == ancestor.id ?
            root.children[1] : root.children[0]

        swap(&nodeToSwap, &otherSubtree)
        swap(&nodeToSwap?.parent, &otherSubtree?.parent)

        if stable {
            swap(&root.children[0], &root.children[1])
        }

        root.recalcSizePosRecursive()
    }

    func preselectDirection(_ direction: String) {
        switch direction.first {
        case "u", "t":
            overrideDirection = .up
        case "d", "b":
            overrideDirection = .down
        case "r":
            overrideDirection = .right
        case "l":
            overrideDirection = .left
        default:
            overrideDirection = .default
        }
    }

    // MARK: - Helper Methods

    private func getNode(for window: WindowElement) -> DwindleNode? {
        return nodes.first { $0.window?.id == window.id && !$0.isNode }
    }

    private func getMasterNode(workspaceID: UUID) -> DwindleNode? {
        return nodes.first { $0.parent == nil && $0.workspaceID == workspaceID }
    }

    private func getWorkspaceGaps(_ workspace: Workspace) -> WorkspaceGaps {
        return WorkspaceGaps(
            in: workspace.gapsIn ?? gapsConfig.in,
            out: workspace.gapsOut ?? gapsConfig.out
        )
    }

    private func calculateWorkspace(_ workspace: Workspace) {
        guard let monitor = MonitorManager.shared.getMonitor(workspace.monitorID) else { return }

        // Handle fullscreen
        if workspace.hasFullscreenWindow,
           let fullscreenWindow = workspace.fullscreenWindow {
            applyFullscreen(window: fullscreenWindow, mode: workspace.fullscreenMode, monitor: monitor)
            return
        }

        // Calculate normal layout
        guard let rootNode = getMasterNode(workspaceID: workspace.id) else { return }

        rootNode.box = CGRect(
            origin: monitor.frame.origin + monitor.reservedTopLeft,
            size: monitor.frame.size - monitor.reservedTopLeft - monitor.reservedBottomRight
        )

        rootNode.recalcSizePosRecursive()
    }

    private func applyNodeDataToWindow(_ node: DwindleNode, force: Bool = false) {
        guard !node.isNode,
              let window = node.window else { return }

        let monitor = getMonitor(for: window)
        let workspace = WorkspaceManager.shared.getWorkspace(node.workspaceID)
        let gaps = getWorkspaceGaps(workspace)
        let edges = getNodeDisplayEdgeFlags(node.box, monitor: monitor)

        var calcBox = node.box

        // Apply gaps
        let gapTopLeft = CGPoint(
            x: edges.left ? gaps.out.left : gaps.in.left,
            y: edges.top ? gaps.out.top : gaps.in.top
        )
        let gapBottomRight = CGPoint(
            x: edges.right ? gaps.out.right : gaps.in.right,
            y: edges.bottom ? gaps.out.bottom : gaps.in.bottom
        )

        calcBox.origin.x += gapTopLeft.x
        calcBox.origin.y += gapTopLeft.y
        calcBox.size.width -= (gapTopLeft.x + gapBottomRight.x)
        calcBox.size.height -= (gapTopLeft.y + gapBottomRight.y)

        // Apply single window aspect ratio
        if node.parent == nil && config.singleWindowAspectRatio != .zero {
            calcBox = applyAspectRatio(box: calcBox, monitor: monitor)
        }

        // Handle pseudotiling
        if window.isPseudotiled {
            calcBox = applyPseudotiling(window: window, box: calcBox)
        }

        // Apply size constraints
        if let minSize = window.minSize {
            calcBox.size.width = max(calcBox.size.width, minSize.width)
            calcBox.size.height = max(calcBox.size.height, minSize.height)
        }
        if let maxSize = window.maxSize {
            calcBox.size.width = min(calcBox.size.width, maxSize.width)
            calcBox.size.height = min(calcBox.size.height, maxSize.height)
        }

        // Set window frame
        window.setFrame(calcBox, animate: !force)
    }
}
```

### 5.2 Node Recalculation

```swift
extension DwindleNode {
    func recalcSizePosRecursive(force: Bool = false,
                               horizontalOverride: Bool = false,
                               verticalOverride: Bool = false) {
        guard children[0] != nil else {
            // Leaf node - apply to window
            layout?.applyNodeDataToWindow(self, force: force)
            return
        }

        let workspace = WorkspaceManager.shared.getWorkspace(workspaceID)
        let monitor = MonitorManager.shared.getMonitor(workspace.monitorID)
        let gaps = layout?.getWorkspaceGaps(workspace)
        let edges = layout?.getNodeDisplayEdgeFlags(box, monitor: monitor)

        // Calculate available size with gaps
        let availableSize = calculateAvailableSize(
            box: box,
            edges: edges!,
            gapsIn: gaps!.in,
            gapsOut: gaps!.out
        )

        // Determine split direction
        if !layout!.config.preserveSplit && !layout!.config.smartSplit {
            splitTop = availableSize.height * CGFloat(layout!.config.splitWidthMultiplier) > availableSize.width
        }

        if verticalOverride {
            splitTop = true
        } else if horizontalOverride {
            splitTop = false
        }

        let splitSide = !splitTop

        if splitSide {
            // Horizontal split (left/right)
            let gapsChild1 = (edges!.left ? gaps!.out.left : gaps!.in.left / 2) + gaps!.in.right / 2
            let gapsChild2 = gaps!.in.left / 2 + (edges!.right ? gaps!.out.right : gaps!.in.right / 2)
            let totalGaps = gapsChild1 + gapsChild2
            let totalAvailable = box.width - totalGaps

            let child1Available = totalAvailable * CGFloat(splitRatio / 2.0)
            let firstSize = child1Available + gapsChild1

            children[0]?.box = CGRect(x: box.minX, y: box.minY, width: firstSize, height: box.height)
            children[1]?.box = CGRect(x: box.minX + firstSize, y: box.minY,
                                     width: box.width - firstSize, height: box.height)
        } else {
            // Vertical split (top/bottom)
            let gapsChild1 = (edges!.top ? gaps!.out.top : gaps!.in.top / 2) + gaps!.in.bottom / 2
            let gapsChild2 = gaps!.in.top / 2 + (edges!.bottom ? gaps!.out.bottom : gaps!.in.bottom / 2)
            let totalGaps = gapsChild1 + gapsChild2
            let totalAvailable = box.height - totalGaps

            let child1Available = totalAvailable * CGFloat(splitRatio / 2.0)
            let firstSize = child1Available + gapsChild1

            children[0]?.box = CGRect(x: box.minX, y: box.minY, width: box.width, height: firstSize)
            children[1]?.box = CGRect(x: box.minX, y: box.minY + firstSize,
                                     width: box.width, height: box.height - firstSize)
        }

        // Recurse to children
        children[0]?.recalcSizePosRecursive(force: force)
        children[1]?.recalcSizePosRecursive(force: force)
    }
}
```

---

## 6. Configuration Management

### 6.1 Config File Format (TOML)

```toml
# ~/.config/dwindle-wm/config.toml

[general]
gaps_in = { top = 5, right = 5, bottom = 5, left = 5 }
gaps_out = { top = 10, right = 10, bottom = 10, left = 10 }

[dwindle]
pseudotile = false
force_split = 0  # 0=cursor, 1=left/top, 2=right/bottom
permanent_direction_override = false
preserve_split = false
special_scale_factor = 0.8
split_width_multiplier = 1.0
use_active_for_splits = true
default_split_ratio = 1.0
split_bias = false
smart_split = false
smart_resizing = true
precise_mouse_move = false
single_window_aspect_ratio = [0, 0]  # [width, height], 0 = disabled
single_window_aspect_ratio_tolerance = 0.1

[workspace_rules]
# Workspace-specific gap overrides
workspace_1 = { gaps_in = { top = 0, right = 0, bottom = 0, left = 0 } }

[window_rules]
# Per-application window rules
[[window_rules.rules]]
match = { app = "Terminal" }
floating = false
pseudotile = false

[[window_rules.rules]]
match = { app = "Calculator" }
floating = true

[keybindings]
# Global hotkeys
focus_left = "Ctrl+Alt+h"
focus_right = "Ctrl+Alt+l"
focus_up = "Ctrl+Alt+k"
focus_down = "Ctrl+Alt+j"

move_left = "Ctrl+Alt+Shift+h"
move_right = "Ctrl+Alt+Shift+l"
move_up = "Ctrl+Alt+Shift+k"
move_down = "Ctrl+Alt+Shift+j"

toggle_split = "Ctrl+Alt+t"
swap_split = "Ctrl+Alt+s"
toggle_floating = "Ctrl+Alt+f"
toggle_fullscreen = "Ctrl+Alt+Return"

resize_increase = "Ctrl+Alt+="
resize_decrease = "Ctrl+Alt+-"

preselect_up = "Ctrl+Alt+Cmd+k"
preselect_down = "Ctrl+Alt+Cmd+j"
preselect_left = "Ctrl+Alt+Cmd+h"
preselect_right = "Ctrl+Alt+Cmd+l"

workspace_1 = "Ctrl+Alt+1"
workspace_2 = "Ctrl+Alt+2"
# ... etc
```

### 6.2 ConfigManager

```swift
class ConfigManager {
    static let shared = ConfigManager()

    private let configPath = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent(".config/dwindle-wm/config.toml")

    var dwindleConfig: DwindleConfig = DwindleConfig()
    var gapsConfig: GapsConfig = GapsConfig()
    var workspaceRules: [UUID: WorkspaceRule] = [:]
    var windowRules: [WindowRule] = []
    var keybindings: Keybindings = Keybindings()

    func load() {
        guard let data = try? Data(contentsOf: configPath),
              let config = try? TOMLDecoder().decode(Config.self, from: data) else {
            // Use defaults
            return
        }

        dwindleConfig = config.dwindle
        gapsConfig = config.general.gaps
        workspaceRules = config.workspaceRules
        windowRules = config.windowRules
        keybindings = config.keybindings
    }

    func save() {
        let config = Config(
            general: GeneralConfig(gaps: gapsConfig),
            dwindle: dwindleConfig,
            workspaceRules: workspaceRules,
            windowRules: windowRules,
            keybindings: keybindings
        )

        guard let data = try? TOMLEncoder().encode(config) else { return }
        try? data.write(to: configPath)
    }

    func reload() {
        load()
        // Notify layout engine
        NotificationCenter.default.post(name: .configReloaded, object: nil)
    }
}
```

---

## 7. Workspace & Monitor Management

### 7.1 WorkspaceManager

```swift
class WorkspaceManager {
    static let shared = WorkspaceManager()

    private var workspaces: [UUID: Workspace] = [:]
    private var activeWorkspacePerMonitor: [UUID: UUID] = [:]

    func createWorkspace(name: String, monitorID: UUID) -> Workspace {
        let workspace = Workspace(
            id: UUID(),
            name: name,
            monitorID: monitorID
        )
        workspaces[workspace.id] = workspace
        return workspace
    }

    func switchToWorkspace(_ workspaceID: UUID, on monitorID: UUID) {
        activeWorkspacePerMonitor[monitorID] = workspaceID

        // Hide all windows not on this workspace
        for (_, workspace) in workspaces where workspace.monitorID == monitorID {
            let shouldShow = workspace.id == workspaceID

            for windowID in workspace.windows {
                if let window = findWindow(id: windowID) {
                    if shouldShow {
                        window.show()
                    } else {
                        window.hide()
                    }
                }
            }
        }

        // Recalculate layout for the workspace
        LayoutEngine.shared.recalculateWorkspace(workspaceID)
    }

    func moveWindowToWorkspace(_ window: WindowElement, workspaceID: UUID) {
        // Remove from current workspace
        if let currentWorkspace = workspaces[window.workspaceID] {
            currentWorkspace.windows.remove(window.id)
        }

        // Add to new workspace
        if let newWorkspace = workspaces[workspaceID] {
            newWorkspace.windows.insert(window.id)
            window.workspaceID = workspaceID
            window.monitorID = newWorkspace.monitorID
        }

        // Hide window if new workspace is not active
        let isActive = activeWorkspacePerMonitor[window.monitorID] == workspaceID
        if isActive {
            window.show()
        } else {
            window.hide()
        }
    }

    func getWorkspace(_ id: UUID) -> Workspace? {
        return workspaces[id]
    }
}
```

### 7.2 MonitorManager

```swift
class MonitorManager {
    static let shared = MonitorManager()

    private var monitors: [UUID: Monitor] = [:]

    func initialize() {
        // Detect all displays
        let maxDisplays: UInt32 = 16
        var displayIDs = [CGDirectDisplayID](repeating: 0, count: Int(maxDisplays))
        var displayCount: UInt32 = 0

        guard CGGetActiveDisplayList(maxDisplays, &displayIDs, &displayCount) == .success else {
            return
        }

        for i in 0..<Int(displayCount) {
            let displayID = displayIDs[i]
            let bounds = CGDisplayBounds(displayID)

            let monitor = Monitor(
                id: UUID(),
                displayID: displayID,
                frame: bounds,
                visibleFrame: calculateVisibleFrame(bounds),
                reservedTopLeft: calculateReservedTopLeft(displayID),
                reservedBottomRight: calculateReservedBottomRight(displayID),
                activeWorkspaceID: UUID(),
                workspaces: [],
                name: getDisplayName(displayID),
                isMain: CGDisplayIsMain(displayID) != 0
            )

            monitors[monitor.id] = monitor
        }

        // Start monitoring for display changes
        CGDisplayRegisterReconfigurationCallback({ _, flags, _ in
            if flags.contains(.addFlag) || flags.contains(.removeFlag) {
                MonitorManager.shared.refreshMonitors()
            }
        }, nil)
    }

    func getMonitor(_ id: UUID) -> Monitor? {
        return monitors[id]
    }

    func getMonitor(at point: CGPoint) -> Monitor? {
        return monitors.values.first { $0.frame.contains(point) }
    }

    private func calculateVisibleFrame(_ bounds: CGRect) -> CGRect {
        // Account for menu bar on main display
        if bounds.origin.y == 0 {
            let menuBarHeight: CGFloat = 24
            return CGRect(
                x: bounds.origin.x,
                y: bounds.origin.y + menuBarHeight,
                width: bounds.width,
                height: bounds.height - menuBarHeight
            )
        }
        return bounds
    }

    private func calculateReservedTopLeft(_ displayID: CGDirectDisplayID) -> CGPoint {
        // Menu bar on main display
        if CGDisplayIsMain(displayID) != 0 {
            return CGPoint(x: 0, y: 24)
        }
        return .zero
    }

    private func calculateReservedBottomRight(_ displayID: CGDirectDisplayID) -> CGPoint {
        // Dock size (need to detect dynamically)
        // For now, assume no dock or auto-hide
        return .zero
    }
}
```

---

## 8. Hotkey Management

### 8.1 HotkeyManager

```swift
class HotkeyManager {
    static let shared = HotkeyManager()

    private var registeredHotkeys: [String: EventHotKeyRef] = [:]

    func register(_ binding: KeyBinding, action: @escaping () -> Void) {
        let hotKeyID = EventHotKeyID(
            signature: OSType("DWND".fourCharCodeValue),
            id: UInt32(binding.hashValue)
        )

        var eventHotKey: EventHotKeyRef?
        let modifiers = convertModifiers(binding.modifiers)
        let keyCode = convertKeyCode(binding.key)

        let status = RegisterEventHotKey(
            keyCode,
            modifiers,
            hotKeyID,
            GetApplicationEventTarget(),
            0,
            &eventHotKey
        )

        if status == noErr, let hotKey = eventHotKey {
            registeredHotkeys[binding.identifier] = hotKey

            // Store action
            HotkeyActions.shared.setAction(for: binding.identifier, action: action)
        }
    }

    func unregister(_ identifier: String) {
        if let hotKey = registeredHotkeys[identifier] {
            UnregisterEventHotKey(hotKey)
            registeredHotkeys.removeValue(forKey: identifier)
            HotkeyActions.shared.removeAction(for: identifier)
        }
    }

    func registerAllFromConfig() {
        let config = ConfigManager.shared.keybindings

        // Focus hotkeys
        register(config.focusLeft) {
            LayoutEngine.shared.focusWindow(direction: .left)
        }
        register(config.focusRight) {
            LayoutEngine.shared.focusWindow(direction: .right)
        }
        register(config.focusUp) {
            LayoutEngine.shared.focusWindow(direction: .up)
        }
        register(config.focusDown) {
            LayoutEngine.shared.focusWindow(direction: .down)
        }

        // Move hotkeys
        register(config.moveLeft) {
            LayoutEngine.shared.moveWindow(direction: "left")
        }
        register(config.moveRight) {
            LayoutEngine.shared.moveWindow(direction: "right")
        }
        register(config.moveUp) {
            LayoutEngine.shared.moveWindow(direction: "up")
        }
        register(config.moveDown) {
            LayoutEngine.shared.moveWindow(direction: "down")
        }

        // Layout messages
        register(config.toggleSplit) {
            LayoutEngine.shared.toggleSplit()
        }
        register(config.swapSplit) {
            LayoutEngine.shared.swapSplit()
        }
        register(config.toggleFloating) {
            LayoutEngine.shared.toggleFloating()
        }
        register(config.toggleFullscreen) {
            LayoutEngine.shared.toggleFullscreen()
        }

        // Resize
        register(config.resizeIncrease) {
            LayoutEngine.shared.resizeActiveWindow(delta: 0.05, exact: false)
        }
        register(config.resizeDecrease) {
            LayoutEngine.shared.resizeActiveWindow(delta: -0.05, exact: false)
        }

        // Preselect
        register(config.preselectUp) {
            LayoutEngine.shared.preselectDirection("up")
        }
        register(config.preselectDown) {
            LayoutEngine.shared.preselectDirection("down")
        }
        register(config.preselectLeft) {
            LayoutEngine.shared.preselectDirection("left")
        }
        register(config.preselectRight) {
            LayoutEngine.shared.preselectDirection("right")
        }

        // Workspaces
        for (index, binding) in config.workspaceBindings.enumerated() {
            let workspaceIndex = index
            register(binding) {
                WorkspaceManager.shared.switchToWorkspace(index: workspaceIndex)
            }
        }
    }

    private func convertModifiers(_ mods: [KeyModifier]) -> UInt32 {
        var result: UInt32 = 0
        for mod in mods {
            switch mod {
            case .command: result |= UInt32(cmdKey)
            case .option: result |= UInt32(optionKey)
            case .control: result |= UInt32(controlKey)
            case .shift: result |= UInt32(shiftKey)
            }
        }
        return result
    }

    private func convertKeyCode(_ key: String) -> UInt32 {
        // Map string keys to Carbon key codes
        // This is a simplified version
        let keyMap: [String: UInt32] = [
            "h": 0x04, "j": 0x26, "k": 0x28, "l": 0x25,
            "1": 0x12, "2": 0x13, "3": 0x14,
            // ... etc
        ]
        return keyMap[key.lowercased()] ?? 0
    }
}
```

---

## 9. User Interface

### 9.1 Menu Bar Application

```swift
@main
struct DwindleApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    var body: some Scene {
        Settings {
            PreferencesView()
        }
    }
}

class AppDelegate: NSObject, NSApplicationDelegate {
    var statusItem: NSStatusItem?
    var popover: NSPopover?

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Check accessibility permissions
        if !AXBridge.shared.checkAccessibility() {
            showPermissionAlert()
            AXBridge.shared.requestAccessibility()
        }

        // Initialize managers
        ConfigManager.shared.load()
        MonitorManager.shared.initialize()
        WorkspaceManager.shared.initialize()

        // Start layout engine
        LayoutEngine.shared.start()

        // Register hotkeys
        HotkeyManager.shared.registerAllFromConfig()

        // Create status bar item
        setupStatusBar()
    }

    private func setupStatusBar() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)

        if let button = statusItem?.button {
            button.image = NSImage(systemSymbolName: "rectangle.split.3x3", accessibilityDescription: "Dwindle")
            button.action = #selector(statusBarButtonClicked)
        }

        // Create menu
        let menu = NSMenu()
        menu.addItem(NSMenuItem(title: "Preferences...", action: #selector(openPreferences), keyEquivalent: ","))
        menu.addItem(NSMenuItem.separator())
        menu.addItem(NSMenuItem(title: "Reload Config", action: #selector(reloadConfig), keyEquivalent: "r"))
        menu.addItem(NSMenuItem(title: "Enable/Disable", action: #selector(toggleEnabled), keyEquivalent: ""))
        menu.addItem(NSMenuItem.separator())
        menu.addItem(NSMenuItem(title: "Quit", action: #selector(quit), keyEquivalent: "q"))

        statusItem?.menu = menu
    }

    @objc private func openPreferences() {
        NSApp.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    @objc private func reloadConfig() {
        ConfigManager.shared.reload()
        HotkeyManager.shared.unregisterAll()
        HotkeyManager.shared.registerAllFromConfig()
        LayoutEngine.shared.recalculateAll()
    }

    @objc private func toggleEnabled() {
        LayoutEngine.shared.toggleEnabled()
    }

    @objc private func quit() {
        NSApp.terminate(nil)
    }

    private func showPermissionAlert() {
        let alert = NSAlert()
        alert.messageText = "Accessibility Permission Required"
        alert.informativeText = "Dwindle needs accessibility permissions to manage windows. Please grant access in System Preferences."
        alert.alertStyle = .warning
        alert.addButton(withTitle: "Open System Preferences")
        alert.addButton(withTitle: "Quit")

        if alert.runModal() == .alertFirstButtonReturn {
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!)
        } else {
            NSApp.terminate(nil)
        }
    }
}
```

### 9.2 Preferences Window

```swift
struct PreferencesView: View {
    @StateObject private var configManager = ConfigManager.shared

    var body: some View {
        TabView {
            GeneralSettingsView()
                .tabItem {
                    Label("General", systemImage: "gear")
                }

            DwindleSettingsView()
                .tabItem {
                    Label("Dwindle", systemImage: "rectangle.split.3x3")
                }

            GapsSettingsView()
                .tabItem {
                    Label("Gaps", systemImage: "square.split.2x2")
                }

            KeybindingsView()
                .tabItem {
                    Label("Keybindings", systemImage: "keyboard")
                }

            WindowRulesView()
                .tabItem {
                    Label("Window Rules", systemImage: "list.bullet.rectangle")
                }
        }
        .frame(width: 600, height: 500)
    }
}

struct DwindleSettingsView: View {
    @ObservedObject var config = ConfigManager.shared.dwindleConfig

    var body: some View {
        Form {
            Section("Split Behavior") {
                Picker("Force Split", selection: $config.forceSplit) {
                    Text("Cursor Position").tag(ForceSplitMode.cursor)
                    Text("Always Left/Top").tag(ForceSplitMode.leftTop)
                    Text("Always Right/Bottom").tag(ForceSplitMode.rightBottom)
                }

                Toggle("Permanent Direction Override", isOn: $config.permanentDirectionOverride)
                Toggle("Preserve Split", isOn: $config.preserveSplit)
                Toggle("Smart Split", isOn: $config.smartSplit)
                Toggle("Split Bias", isOn: $config.splitBias)
            }

            Section("Split Parameters") {
                HStack {
                    Text("Split Width Multiplier")
                    Spacer()
                    TextField("", value: $config.splitWidthMultiplier, format: .number)
                        .frame(width: 60)
                }

                HStack {
                    Text("Default Split Ratio")
                    Spacer()
                    Slider(value: $config.defaultSplitRatio, in: 0.1...1.9)
                    Text(String(format: "%.2f", config.defaultSplitRatio))
                        .frame(width: 40)
                }
            }

            Section("Window Behavior") {
                Toggle("Pseudotile", isOn: $config.pseudotile)
                Toggle("Use Active For Splits", isOn: $config.useActiveForSplits)
                Toggle("Smart Resizing", isOn: $config.smartResizing)
                Toggle("Precise Mouse Move", isOn: $config.preciseMouseMove)
            }

            Section("Special Workspace") {
                HStack {
                    Text("Scale Factor")
                    Spacer()
                    Slider(value: $config.specialScaleFactor, in: 0.1...1.0)
                    Text(String(format: "%.2f", config.specialScaleFactor))
                        .frame(width: 40)
                }
            }

            Section("Single Window Aspect Ratio") {
                HStack {
                    Text("Width")
                    TextField("", value: $config.singleWindowAspectRatio.width, format: .number)
                        .frame(width: 60)
                    Text("Height")
                    TextField("", value: $config.singleWindowAspectRatio.height, format: .number)
                        .frame(width: 60)
                    Text("(0 to disable)")
                        .foregroundColor(.secondary)
                }

                HStack {
                    Text("Tolerance")
                    Spacer()
                    Slider(value: $config.singleWindowAspectRatioTolerance, in: 0.0...1.0)
                    Text(String(format: "%.2f", config.singleWindowAspectRatioTolerance))
                        .frame(width: 40)
                }
            }
        }
        .padding()
    }
}
```

---

## 10. Testing Strategy

### 10.1 Unit Tests

```swift
class DwindleLayoutTests: XCTestCase {
    var layout: DwindleLayout!

    override func setUp() {
        layout = DwindleLayout()
    }

    func testSingleWindowFullscreen() {
        // Test that first window takes full space
        let window = createMockWindow()
        layout.onWindowCreatedTiling(window)

        let node = layout.getNode(for: window)
        XCTAssertNotNil(node)
        XCTAssertNil(node?.parent)
    }

    func testTwoWindowsSplit() {
        // Test that two windows split correctly
        let window1 = createMockWindow()
        let window2 = createMockWindow()

        layout.onWindowCreatedTiling(window1)
        layout.onWindowCreatedTiling(window2)

        let node1 = layout.getNode(for: window1)
        let node2 = layout.getNode(for: window2)

        XCTAssertNotNil(node1?.parent)
        XCTAssertNotNil(node2?.parent)
        XCTAssertEqual(node1?.parent?.id, node2?.parent?.id)
    }

    func testWindowRemoval() {
        // Test that removing window promotes sibling
        let window1 = createMockWindow()
        let window2 = createMockWindow()

        layout.onWindowCreatedTiling(window1)
        layout.onWindowCreatedTiling(window2)
        layout.onWindowRemovedTiling(window2)

        let node1 = layout.getNode(for: window1)
        XCTAssertNil(node1?.parent)
    }

    func testSplitRatio() {
        // Test split ratio adjustment
        let window1 = createMockWindow()
        let window2 = createMockWindow()

        layout.onWindowCreatedTiling(window1)
        layout.onWindowCreatedTiling(window2)

        layout.alterSplitRatio(window1, delta: 0.2, exact: false)

        let node = layout.getNode(for: window1)
        XCTAssertEqual(node?.parent?.splitRatio, 1.2, accuracy: 0.01)
    }

    func testSmartSplit() {
        // Test smart split based on cursor position
        layout.config.smartSplit = true
        // ... test implementation
    }

    func testForceSplit() {
        // Test force split modes
        layout.config.forceSplit = .leftTop
        // ... test implementation
    }

    func testPreserveSplit() {
        // Test preserve split
        layout.config.preserveSplit = true
        // ... test implementation
    }
}
```

### 10.2 Integration Tests

```swift
class IntegrationTests: XCTestCase {
    func testFullWorkflow() {
        // Test complete workflow: create, arrange, remove windows
    }

    func testMultipleWorkspaces() {
        // Test workspace switching and window movement
    }

    func testMultipleMonitors() {
        // Test multi-monitor setup
    }

    func testConfigReload() {
        // Test hot-reloading configuration
    }
}
```

---

## 11. Implementation Phases

### Phase 1: Foundation (Week 1-2)
- [ ] Project setup with Swift Package Manager
- [ ] AXBridge implementation
- [ ] WindowObserver implementation
- [ ] Basic data structures (Node, Window, Workspace, Monitor)
- [ ] MonitorManager implementation
- [ ] Basic window enumeration and tracking

### Phase 2: Core Layout (Week 3-4)
- [ ] DwindleLayout basic algorithm
- [ ] Node tree manipulation (create, remove, recalc)
- [ ] Split logic (cursor-based, force split)
- [ ] Gap calculation
- [ ] Window frame application

### Phase 3: Advanced Features (Week 5-6)
- [ ] Smart split
- [ ] Smart resizing
- [ ] Pseudotiling
- [ ] Fullscreen support
- [ ] Single window aspect ratio
- [ ] Split ratio adjustment

### Phase 4: Workspace Management (Week 7)
- [ ] WorkspaceManager implementation
- [ ] Virtual workspace support
- [ ] Window showing/hiding on workspace switch
- [ ] Per-workspace configuration

### Phase 5: Configuration (Week 8)
- [ ] ConfigManager implementation
- [ ] TOML parser integration
- [ ] Window rules system
- [ ] Workspace rules system
- [ ] Config hot-reloading

### Phase 6: UI & Hotkeys (Week 9)
- [ ] Menu bar application
- [ ] Preferences window
- [ ] HotkeyManager implementation
- [ ] All hotkey bindings
- [ ] Permission handling UI

### Phase 7: Layout Messages (Week 10)
- [ ] togglesplit
- [ ] swapsplit
- [ ] movetoroot
- [ ] preselect
- [ ] moveWindowTo
- [ ] switchWindows

### Phase 8: Testing & Polish (Week 11-12)
- [ ] Unit tests for all core functionality
- [ ] Integration tests
- [ ] Bug fixes
- [ ] Performance optimization
- [ ] Documentation
- [ ] User guide

---

## 12. Limitations & Workarounds

### 12.1 Known macOS Limitations

1. **No Animation Control**
   - **Limitation**: Cannot control window animations (no compositor access)
   - **Workaround**: Instant window positioning, rely on macOS default animations

2. **Some Apps Don't Support AX API Well**
   - **Limitation**: Some apps (games, fullscreen apps) may not respond to AX commands
   - **Workaround**: Maintain blacklist of incompatible apps, allow user to add apps to floating list

3. **No Window Hiding API**
   - **Limitation**: Cannot truly hide windows, only minimize or move off-screen
   - **Workaround**: Move windows to far off-screen coordinates when on inactive workspaces

4. **No Window Grouping API**
   - **Limitation**: Cannot create window groups/tabs like Hyprland
   - **Workaround**: This feature will be omitted (acceptable as not core dwindle)

5. **Dock/Menu Bar Interference**
   - **Limitation**: Dock and menu bar can interfere with window positioning
   - **Workaround**: Properly calculate reserved areas and respect them

6. **Permission Required**
   - **Limitation**: Requires accessibility permissions
   - **Workaround**: Clear UI to request permissions, detect and guide user

---

## 13. Performance Considerations

### 13.1 Optimization Strategies

1. **Lazy Recalculation**
   - Only recalculate affected subtrees
   - Batch multiple changes before recalculating
   - Debounce rapid changes

2. **Caching**
   - Cache window frames
   - Cache monitor configurations
   - Cache workspace states

3. **Efficient Data Structures**
   - Use hash maps for O(1) lookups
   - Use sets for window collections
   - Use weak references to avoid cycles

4. **Async Operations**
   - Window enumeration on background thread
   - Config file I/O on background thread
   - Only window positioning on main thread

---

## 14. Future Enhancements

### 14.1 Possible Extensions

1. **Scripting Support**
   - Expose IPC socket for external control
   - Allow scripts to query and manipulate layout
   - Compatible with Hyprland dispatcher format

2. **Window Rules Enhancement**
   - Support regex matching
   - Support window title matching
   - Support workspace-specific rules

3. **Visual Feedback**
   - Show preselect direction on screen
   - Show split preview when opening window
   - Show gaps/borders overlay

4. **Advanced Workspace Features**
   - Workspace groups
   - Workspace persistence across restarts
   - Workspace-specific wallpapers (via external tool)

5. **Gestures Support**
   - Trackpad gestures for window operations
   - Mouse gestures for quick actions

---

## 15. Conclusion

This technical architecture provides a complete roadmap for implementing a macOS window manager with 100% feature parity to Hyprland's dwindle layout algorithm. The design:

- Uses only public Accessibility APIs (no SIP disabling required)
- Implements all dwindle configuration options
- Supports all layout messages and operations
- Provides workspace and multi-monitor support
- Includes comprehensive configuration system
- Uses native Swift and SwiftUI for maintainability

The phased implementation approach ensures steady progress with testable milestones. While some limitations exist due to macOS constraints, the workarounds provide acceptable user experience without compromising the core dwindle functionality.

**Estimated Development Time**: 12 weeks for full implementation
**Lines of Code (estimated)**: ~15,000-20,000 LOC
**Dependencies**: Minimal (TOML parser, Swift standard library)

This architecture ensures the macOS dwindle window manager will be a production-ready, feature-complete implementation that brings the beloved Hyprland tiling experience to macOS users.
