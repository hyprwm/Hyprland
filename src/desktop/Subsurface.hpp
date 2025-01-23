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
    CSubsurface(CPopup* pOwner);

    // real nodes
    CSubsurface(SP<CWLSubsurfaceResource> pSubsurface, PHLWINDOW pOwner);
    CSubsurface(SP<CWLSubsurfaceResource> pSubsurface, CPopup* pOwner);

    ~CSubsurface();

    Vector2D coordsRelativeToParent();
    Vector2D coordsGlobal();

    Vector2D size();

    void     onCommit();
    void     onDestroy();
    void     onNewSubsurface(SP<CWLSubsurfaceResource> pSubsurface);
    void     onMap();
    void     onUnmap();

    bool     visible();

    void     recheckDamageForSubsurfaces();

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
    CSubsurface*                 m_pParent = nullptr;

    PHLWINDOWREF                 m_pWindowParent;
    CPopup*                      m_pPopupParent = nullptr;

    std::vector<UP<CSubsurface>> m_vChildren;

    bool                         m_bInert = false;

    void                         initSignals();
    void                         initExistingSubsurfaces(SP<CWLSurfaceResource> pSurface);
    void                         checkSiblingDamage();
};