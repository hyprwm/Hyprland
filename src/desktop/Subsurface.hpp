#pragma once

#include "../defines.hpp"
#include <vector>
#include "WLSurface.hpp"

class CPopup;
class CWLSubsurfaceResource;

class CSubsurface {
  public:
    // root dummy nodes
    CSubsurface(PHLWINDOW pOwner);
    CSubsurface(WP<CPopup> pOwner);

    // real nodes
    CSubsurface(SP<CWLSubsurfaceResource> pSubsurface, PHLWINDOW pOwner);
    CSubsurface(SP<CWLSubsurfaceResource> pSubsurface, WP<CPopup> pOwner);

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
    struct {
        CHyprSignalListener destroySubsurface;
        CHyprSignalListener commitSubsurface;
        CHyprSignalListener mapSubsurface;
        CHyprSignalListener unmapSubsurface;
        CHyprSignalListener newSubsurface;
    } listeners;

    WP<CWLSubsurfaceResource> m_pSubsurface;
    SP<CWLSurface>            m_pWLSurface;
    Vector2D                  m_vLastSize = {};

    // if nullptr, means it's a dummy node
    WP<CSubsurface>              m_pParent;

    PHLWINDOWREF                 m_pWindowParent;
    WP<CPopup>                   m_pPopupParent;

    std::vector<UP<CSubsurface>> m_vChildren;

    bool                         m_bInert = false;

    void                         initSignals();
    void                         initExistingSubsurfaces(SP<CWLSurfaceResource> pSurface);
    void                         checkSiblingDamage();
};
