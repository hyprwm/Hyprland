# Render Batching System

## Overview

The render batching system in Hyprland reduces draw calls and state changes by grouping similar rendering operations together. This optimization can provide significant performance improvements, especially in scenes with many similar elements.

## Performance Benefits

Based on the implementation from TODO.md:
- **50-70% reduction in draw calls** for scenes with many similar elements
- **Reduced CPU overhead** from fewer OpenGL state changes  
- **Better GPU utilization** through larger batches

## Architecture

### Core Components

1. **CRenderBatchManager** (`src/render/BatchManager.hpp`)
   - Central manager for batching operations
   - Groups operations by shader state
   - Tracks performance metrics

2. **CBatchedRectRenderer** (`src/render/BatchedRectRenderer.hpp`)
   - Optimized renderer for multiple rectangles
   - Uses vertex buffer objects for efficient rendering
   - Reduces N draw calls to 1 for batched rectangles

3. **CBatchedPassElement** (`src/render/pass/BatchedPassElement.hpp`)
   - Integration with the pass rendering system
   - Allows batching of pass elements

## Usage Guide

### Basic Usage

```cpp
auto* batchManager = g_pHyprOpenGL->getBatchManager();

// Begin batching
batchManager->beginBatch();

// Add operations - these will be grouped by shader state
batchManager->addRect(CBox(0, 0, 100, 100), CHyprColor(1, 0, 0, 1), 0, 2.0f);
batchManager->addRect(CBox(100, 0, 100, 100), CHyprColor(1, 0, 0, 1), 0, 2.0f);

// End batching - all operations are executed
batchManager->endBatch();
```

### Supported Operations

- **Rectangles**: `addRect(box, color, round, roundingPower)`
- **Textures**: `addTexture(textureId, box, alpha, round, roundingPower)`
- **Borders**: `addBorder(box, color, round, roundingPower, borderSize)`
- **Shadows**: `addShadow(box, round, roundingPower, range, color)`

### Performance Metrics

```cpp
// Get metrics after rendering
auto metrics = batchManager->getMetrics();
Debug::log(LOG, "Draw calls: {}", metrics.drawCalls);
Debug::log(LOG, "State changes: {}", metrics.stateChanges);
Debug::log(LOG, "Texture binds: {}", metrics.textureBinds);

// Reset metrics for next frame
batchManager->resetMetrics();
```

## Best Practices

### 1. Group Similar Operations

Operations are batched by shader state (rounding, power). Group similar operations together:

```cpp
// Good - all rectangles with same rounding
batchManager->beginBatch();
for (auto& rect : rectangles) {
    batchManager->addRect(rect.box, rect.color, 10, 2.0f);
}
batchManager->endBatch();

// Less optimal - mixed rounding values
batchManager->beginBatch();
batchManager->addRect(box1, color1, 0, 2.0f);
batchManager->addRect(box2, color2, 10, 2.0f);  // Different state
batchManager->addRect(box3, color3, 0, 2.0f);   // State change again
batchManager->endBatch();
```

### 2. Use Auto-Flush for Large Scenes

```cpp
// Enable auto-flush for complex scenes
batchManager->setAutoFlush(true);
```

### 3. Batch Window Decorations

Window decorations are ideal for batching:

```cpp
batchManager->beginBatch();
for (auto& window : windows) {
    // All borders with same style
    renderWindowBorder(window);
}
batchManager->endBatch();
```

### 4. Monitor Batch Sizes

The system automatically uses optimized rendering for batches > 3 rectangles. Smaller batches use the standard rendering path.

## Implementation Details

### Automatic Optimization

The batch manager automatically chooses between:
- **Optimized path**: For batches with > 3 rectangles
- **Standard path**: For smaller batches

### State Grouping

Operations are grouped by:
- Operation type (rect, texture, border, shadow)
- Rounding value
- Rounding power
- Texture ID (for texture operations)

### Memory Management

- Vertex buffers are allocated dynamically
- Buffers are reused between frames
- No persistent memory overhead when not batching

## Integration Points

### Render Loop Integration

```cpp
void CHyprRenderer::renderWorkspace() {
    auto* batchManager = g_pHyprOpenGL->getBatchManager();
    
    // Batch background elements
    batchManager->beginBatch();
    renderBackgrounds();
    batchManager->endBatch();
    
    // Render complex elements individually
    renderWindows();
    
    // Batch UI elements
    batchManager->beginBatch();
    renderUI();
    batchManager->endBatch();
}
```

### Pass System Integration

```cpp
auto batchedPass = makeShared<CBatchedPassElement>();

// Add multiple pass elements
batchedPass->addElement(rectPass1);
batchedPass->addElement(rectPass2);
batchedPass->addElement(rectPass3);

// Execute as single batched operation
pass->add(batchedPass);
```

## Debugging

### Enable Debug Metrics

```cpp
// In your render loop
if (Debug::trace) {
    auto metrics = batchManager->getMetrics();
    Debug::log(TRACE, "Batch efficiency: {} ops in {} draws", 
               metrics.pendingOperations, metrics.drawCalls);
}
```

### Visual Debugging

Different batch groups can be visualized by slightly modifying colors:

```cpp
// Debug mode - tint each batch differently
static int batchColorIndex = 0;
float tint = 0.1f * (batchColorIndex++ % 10);
color.r += tint;
```

## Future Enhancements

1. **Instanced Rendering**: Use GPU instancing for even better performance
2. **Texture Atlasing**: Batch different textures using atlas
3. **Compute Shader Integration**: Process batches with compute shaders
4. **Dynamic Batch Sizing**: Adjust batch size based on GPU capabilities