#pragma once

#include "../defines.hpp"
#include "../render/Texture.hpp"
#include "../helpers/AnimatedVariable.hpp"

#include <cairo/cairo.h>

class CHyprError {
  public:
    CHyprError();
    ~CHyprError();

    void  queueCreate(std::string message, const CHyprColor& color);
    void  draw();
    void  destroy();

    bool  active();
    float height(); // logical

  private:
    void              createQueued();
    std::string       m_szQueued = "";
    CHyprColor        m_cQueued;
    bool              m_bQueuedDestroy = false;
    bool              m_bIsCreated     = false;
    SP<CTexture>      m_pTexture;
    PHLANIMVAR<float> m_fFadeOpacity;
    CBox              m_bDamageBox  = {0, 0, 0, 0};
    float             m_fLastHeight = 0.F;

    bool              m_bMonitorChanged = false;
};

inline std::unique_ptr<CHyprError> g_pHyprError; // This is a full-screen error. Treat it with respect, and there can only be one at a time.
