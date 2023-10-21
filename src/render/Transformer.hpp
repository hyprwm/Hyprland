#pragma once

#include "Framebuffer.hpp"

// A window transformer can be attached to a window.
// If any is attached, Hyprland will render the window to a separate fb, then call the transform() func with it,
// and finally render it back to the main fb after all transformers pass.
class IWindowTransformer {
  public:
    // called by Hyprland. For more data about what is being rendered, inspect render data.
    // returns the out fb.
    virtual CFramebuffer* transform(CFramebuffer* in) = 0;
};