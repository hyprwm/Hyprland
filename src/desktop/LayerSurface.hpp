#pragma once

#include <string>
#include "../defines.hpp"
#include "WLSurface.hpp"
#include "../helpers/AnimatedVariable.hpp"

class CLayerShellResource;

class CLayerSurface {
  public:
    static PHLLS create(SP<CLayerShellResource>);

  private:
    CLayerSurface(SP<CLayerShellResource>);

  public:
    ~CLayerSurface();

    void                    applyRules();
    void                    startAnimation(bool in, bool instant = false);
    bool                    isFadedOut();
    int                     popupsCount();

    PHLANIMVAR<Vector2D>    m_realPosition;
    PHLANIMVAR<Vector2D>    m_realSize;
    PHLANIMVAR<float>       m_alpha;

    WP<CLayerShellResource> m_layerSurface;

    // the header providing the enum type cannot be imported here
    int                        m_interactivity = 0;

    SP<CWLSurface>             m_surface;

    bool                       m_mapped = false;
    uint32_t                   m_layer  = 0;

    PHLMONITORREF              m_monitor;

    bool                       m_fadingOut     = false;
    bool                       m_readyToDelete = false;
    bool                       m_noProcess     = false;
    bool                       m_noAnimations  = false;

    bool                       m_forceBlur                   = false;
    bool                       m_forceBlurPopups             = false;
    int64_t                    m_xray                        = -1;
    bool                       m_ignoreAlpha                 = false;
    float                      m_ignoreAlphaValue            = 0.f;
    bool                       m_dimAround                   = false;
    int64_t                    m_order                       = 0;
    bool                       m_aboveLockscreen             = false;
    bool                       m_aboveLockscreenInteractable = false;

    std::optional<std::string> m_animationStyle;

    PHLLSREF                   m_self;

    CBox                       m_geometry = {0, 0, 0, 0};
    Vector2D                   m_position;
    std::string                m_namespace = "";
    UP<CPopup>                 m_popupHead;

    pid_t                      getPID();

    void                       onDestroy();
    void                       onMap();
    void                       onUnmap();
    void                       onCommit();
    MONITORID                  monitorID();

  private:
    struct {
        CHyprSignalListener destroy;
        CHyprSignalListener map;
        CHyprSignalListener unmap;
        CHyprSignalListener commit;
    } m_listeners;

    void registerCallbacks();

    // For the list lookup
    bool operator==(const CLayerSurface& rhs) const {
        return m_layerSurface == rhs.m_layerSurface && m_monitor == rhs.m_monitor;
    }
};

inline bool valid(PHLLS l) {
    return l;
}

inline bool valid(PHLLSREF l) {
    return l;
}

inline bool validMapped(PHLLS l) {
    if (!valid(l))
        return false;
    return l->m_mapped;
}

inline bool validMapped(PHLLSREF l) {
    if (!valid(l))
        return false;
    return l->m_mapped;
}
