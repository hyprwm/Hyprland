# Render Batching Documentation

## Overview

Hyprland's render batching system is a performance optimization feature that reduces the number of OpenGL draw calls by combining similar rendering operations into batches. This can significantly improve performance, especially when rendering many windows or decorations.

## Configuration Options

The render batching system can be configured through the following options in your `hyprland.conf`:

### misc:render_batching
- **Type**: Boolean
- **Default**: `true`
- **Description**: Enable or disable the render batching system entirely.

```conf
misc {
    render_batching = true
}
```

### misc:render_batch_size
- **Type**: Integer
- **Default**: `1000`
- **Range**: `100` - `10000`
- **Description**: Maximum number of render operations to batch together. Higher values use more memory but may improve performance with many windows.

```conf
misc {
    render_batch_size = 2000  # Increase for better batching with many windows
}
```

### misc:render_batch_instancing
- **Type**: Boolean
- **Default**: `true`
- **Description**: Enable GPU instancing for even better performance. Requires OpenGL 3.3+ or OpenGL ES 3.2+.

```conf
misc {
    render_batch_instancing = true
}
```

## How It Works

### Batching
When render batching is enabled, Hyprland collects similar rendering operations (like drawing window borders or shadows) and executes them together in a single draw call instead of individually. This reduces:
- CPU overhead from OpenGL state changes
- Driver overhead from draw call submissions
- GPU pipeline stalls

### GPU Instancing
When both batching and instancing are enabled, Hyprland uses GPU instancing to render multiple instances of the same geometry (like rectangles) in a single draw call. This is the most efficient rendering method but requires modern GPU support.

The system automatically chooses the best rendering method based on batch size:
- **< 4 rectangles**: Immediate mode rendering (no batching)
- **4-20 rectangles**: Standard batched rendering
- **> 20 rectangles**: GPU instanced rendering (if supported)

### Automatic Fallback
If GPU instancing is not supported by your hardware, Hyprland automatically falls back to standard batched rendering, which still provides significant performance improvements over immediate mode rendering. The fallback is seamless and requires no configuration.

## Performance Considerations

### Benefits
- **Reduced draw calls**: Can reduce draw calls by 80-90% in typical scenarios
- **Better GPU utilization**: Fewer state changes mean the GPU can work more efficiently
- **Lower CPU usage**: Less time spent in the OpenGL driver

### Trade-offs
- **Memory usage**: Batching requires buffering operations, using more memory
- **Latency**: Minimal increase in rendering latency (microseconds)
- **Compatibility**: GPU instancing requires modern hardware

## Technical Details

### Supported Operations
The batching system currently supports:
- Rectangle rendering (window backgrounds, borders)
- Shadow rendering (drop shadows)
- Border rendering (window borders)
- Texture rendering (with same texture ID)

### Batching Criteria
Operations are batched together when they share:
- Same operation type (rect, shadow, border, texture)
- Same rounding settings (corner radius and power)
- Same texture ID (for texture operations)

### Instance Data Format
Each instanced rectangle uses 8 floats:
- Position and size: x, y, width, height (4 floats)
- Color: r, g, b, a (4 floats, premultiplied alpha)

## Troubleshooting

### Checking if batching is active
Look for these log messages during Hyprland startup:
```
[LOG] BatchManager: Batching=1, Instancing=1
[LOG] BatchManager: GPU instanced rendering supported
```

If instancing is not supported, you'll see:
```
[WARN] GPU instanced rendering not supported, falling back to batched rendering
```

### Disabling for debugging
If you suspect render batching is causing issues:
```conf
misc {
    render_batching = false  # Disable completely
    # OR
    render_batch_instancing = false  # Disable only GPU instancing
}
```

### Performance tuning
For systems with many windows:
```conf
misc {
    render_batch_size = 5000  # Increase batch size
}
```

For systems with limited memory:
```conf
misc {
    render_batch_size = 500  # Reduce batch size
}
```

## Debug Information

In debug builds, batching efficiency is logged periodically:
```
[LOG] Batching efficiency: 87.5% (10000 ops in 1250 batches)
```

This shows the percentage reduction in draw calls achieved by batching.