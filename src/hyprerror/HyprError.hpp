#pragma once

#include "../defines.hpp"
#include "../render/Texture.hpp"
#include "../helpers/AnimatedVariable.hpp"

#include <cairo/cairo.h>

class CHyprError {
  public:
    CHyprError();
    ~CHyprError() = default;

    void  queueCreate(std::string message, const CHyprColor& color);
    void  draw();
    void  destroy();

    bool  active();
    float height(); // logical

  private:
    void              createQueued();
    std::string       m_queued = "";
    CHyprColor        m_queuedColor;
    bool              m_queuedDestroy = false;
    bool              m_isCreated     = false;
    SP<CTexture>      m_texture;
    PHLANIMVAR<float> m_fadeOpacity;
    CBox              m_damageBox  = {0, 0, 0, 0};
    float             m_lastHeight = 0.F;

    bool              m_monitorChanged = false;
};

inline UP<CHyprError> g_pHyprError; // This is a full-screen error. Treat it with respect, and there can only be one at a time.
