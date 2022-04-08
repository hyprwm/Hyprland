#pragma once

#include "../defines.hpp"
#include "../render/Texture.hpp"

#include <cairo/cairo.h>

class CHyprError {
public:
    void            queueCreate(std::string message, const CColor& color);
    void            draw();
    void            destroy();

private:
    void            createQueued();
    std::string     m_szQueued = "";
    CColor          m_cQueued;
    bool            m_bQueuedDestroy = false;
    bool            m_bIsCreated = false;
    CTexture        m_tTexture;
};

inline std::unique_ptr<CHyprError> g_pHyprError; // This is a full-screen error. Treat it with respect, and there can only be one at a time.