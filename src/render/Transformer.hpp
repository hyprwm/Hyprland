#pragma once

#include "Framebuffer.hpp"
#include "pass/TexPassElement.hpp"

// A window transformer can be attached to a window.
// If any is attached, Hyprland will render the window to a separate fb, then call the transform() func with it,
// and finally render it back to the main fb after all transformers pass.
//
// Worth noting transformers for now only affect the main pass (not popups)
class IWindowTransformer {
  public:
    virtual ~IWindowTransformer() = default;

    // called by Hyprland. For more data about what is being rendered, inspect render data.
    // returns the out fb.
    virtual CFramebuffer* transform(CFramebuffer* in) = 0;

    // called by Hyprland before a window main pass is started.
    virtual void preWindowRender(CTexPassElement::SRenderData* pRenderData);
};