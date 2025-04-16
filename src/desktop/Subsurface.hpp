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

    WP<CSubsurface> m_pSelf;

  private:
    CSubsurface() = default;

    struct {
        CHyprSignalListener destroySubsurface;
        CHyprSignalListener commitSubsurface;
        CHyprSignalListener mapSubsurface;
        CHyprSignalListener unmapSubsurface;
        CHyprSignalListener newSubsurface;
    } listeners;

    WP<CWLSubsurfaceResource> m_pSubsurface;
    SP<CWLSurface>            m_pWLSurface;
    Vector2D                  m_vLastSize     = {};
    Vector2D                  m_vLastPosition = {};

    // if nullptr, means it's a dummy node
    WP<CSubsurface>              m_pParent;

    PHLWINDOWREF                 m_pWindowParent;
    WP<CPopup>                   m_pPopupParent;

    std::vector<UP<CSubsurface>> m_vChildren;

    bool                         m_bInert = false;

    void                         initSignals();
    void                         initExistingSubsurfaces(SP<CWLSurfaceResource> pSurface);
    void                         checkSiblingDamage();
    void                         damageEntireParent();
};
