# GPU Instanced Rendering Implementation for Hyprland

## Overview

This implementation adds GPU instanced rendering capabilities to Hyprland's rendering system, providing significant performance improvements when rendering many similar objects (like window decorations, shadows, and borders).

## Key Components

### 1. InstancedRectRenderer (`src/render/InstancedRectRenderer.cpp/.hpp`)

A specialized renderer that uses GPU instancing to draw multiple rectangles in a single draw call.

**Features:**
- Automatic detection of instancing support (OpenGL ES 3.0+ or GL_ARB_instanced_arrays)
- Efficient vertex buffer management with instance attributes
- Support for up to 10,000 instances per draw call
- Fallback to batched rendering if instancing is not supported

### 2. Enhanced BatchManager (`src/render/BatchManager.cpp/.hpp`)

The BatchManager now intelligently chooses between three rendering modes:
- **Individual rendering**: For small batches (< 4 rectangles)
- **Batched rendering**: For medium batches (4-20 rectangles)
- **Instanced rendering**: For large batches (> 20 rectangles)

### 3. Instanced Shader (`src/render/shaders/glsl/quad_instanced.vert`)

A custom vertex shader that supports per-instance attributes:
- Instance rectangle (position and size)
- Instance color (with premultiplied alpha)

## Performance Benefits

### Draw Call Reduction
- Traditional: 1 draw call per rectangle
- Batched: 1 draw call per batch (4-20 rectangles)
- Instanced: 1 draw call per thousands of rectangles

### Memory Efficiency
- Shared vertex data for all instances (4 vertices defining a unit quad)
- Per-instance data stored compactly (8 floats per instance)
- Reduced CPU-GPU bandwidth usage

### Expected Performance Gains
- 50-90% reduction in draw calls for window-heavy scenarios
- Improved frame rates when many windows are visible
- Lower CPU usage due to fewer state changes

## Usage

### Enable/Disable Instancing
```cpp
// Enable instanced rendering (default)
batchManager.setUseInstancing(true);

// Disable instanced rendering (fallback to batched)
batchManager.setUseInstancing(false);
```

### Performance Monitoring
```cpp
// Print detailed performance report
batchManager.printPerformanceReport();

// Get instancing efficiency (0.0 - 1.0)
float efficiency = batchManager.getInstancedRenderingEfficiency();
```

### Metrics Available
- Total draw calls
- Instanced vs batched draw call breakdown
- Number of instances rendered
- State changes and texture binds
- Rendering efficiency percentage

## Integration with Existing Systems

The instanced rendering seamlessly integrates with:
- Window decoration rendering
- Shadow rendering (via BatchedShadowPassElement)
- Border rendering
- Any rectangle-based rendering operations

## Future Enhancements

1. **Texture Atlasing**: Combine multiple textures to enable instanced texture rendering
2. **Geometry Shaders**: Generate decoration geometry on the GPU
3. **Compute Shaders**: Pre-process instance data for complex effects
4. **Dynamic Batching**: Adaptive threshold selection based on GPU capabilities
5. **Multi-draw Indirect**: Further reduce CPU overhead with indirect drawing

## Testing

A performance test suite is included in `src/render/test/TestInstancedRendering.cpp` that demonstrates:
- Performance comparison between rendering modes
- Efficiency at different batch sizes
- Real-world scenario simulations

## Requirements

- OpenGL ES 3.0+ or OpenGL with GL_ARB_instanced_arrays extension
- GPU with efficient instancing support (most modern GPUs)

## Configuration

No user configuration is required. The system automatically:
- Detects GPU capabilities
- Chooses optimal rendering path
- Falls back gracefully if instancing is unavailable