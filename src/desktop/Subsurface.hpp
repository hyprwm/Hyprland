#pragma once

#include "../defines.hpp"
#include <vector>
#include "WLSurface.hpp"

class CPopup;
class CWLSubsurfaceResource;

class CSubsurface {
  public:
    // root dummy nodes
    static UP<CSubsurface> create(PHLWINDOW pOwner);
    static UP<CSubsurface> create(WP<CPopup> pOwner);

    // real nodes
    static UP<CSubsurface> create(SP<CWLSubsurfaceResource> pSubsurface, PHLWINDOW pOwner);
    static UP<CSubsurface> create(SP<CWLSubsurfaceResource> pSubsurface, WP<CPopup> pOwner);

    ~CSubsurface() = default;

    Vector2D        coordsRelativeToParent();
    Vector2D        coordsGlobal();

    Vector2D        size();

    void            onCommit();
    void            onDestroy();
    void            onNewSubsurface(SP<CWLSubsurfaceResource> pSubsurface);
    void            onMap();
    void            onUnmap();

    bool            visible();

    void            recheckDamageForSubsurfaces();

    WP<CSubsurface> m_self;

  private:
    CSubsurface() = default;

    struct {
        CHyprSignalListener destroySubsurface;
        CHyprSignalListener commitSubsurface;
        CHyprSignalListener mapSubsurface;
        CHyprSignalListener unmapSubsurface;
        CHyprSignalListener newSubsurface;
    } m_listeners;

    WP<CWLSubsurfaceResource> m_subsurface;
    SP<CWLSurface>            m_wlSurface;
    Vector2D                  m_lastSize     = {};
    Vector2D                  m_lastPosition = {};

    // if nullptr, means it's a dummy node
    WP<CSubsurface>              m_parent;

    PHLWINDOWREF                 m_windowParent;
    WP<CPopup>                   m_popupParent;

    std::vector<UP<CSubsurface>> m_children;

    bool                         m_inert = false;

    void                         initSignals();
    void                         initExistingSubsurfaces(SP<CWLSurfaceResource> pSurface);
    void                         checkSiblingDamage();
    void                         damageLastArea();
};
