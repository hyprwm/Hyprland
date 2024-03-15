#pragma once

#include <vector>
#include <memory>
#include "Subsurface.hpp"

struct SLayerSurface;

class CPopup {
  public:
    // dummy head nodes
    CPopup(CWindow* pOwner);
    CPopup(SLayerSurface* pOwner);

    // real nodes
    CPopup(wlr_xdg_popup* popup, CPopup* pOwner);

    ~CPopup();

    Vector2D   coordsRelativeToParent();
    Vector2D   coordsGlobal();

    Vector2D   size();

    void       onNewPopup(wlr_xdg_popup* popup);
    void       onDestroy();
    void       onMap();
    void       onUnmap();
    void       onCommit(bool ignoreSiblings = false);
    void       onReposition();

    void       recheckTree();

    CWLSurface m_sWLSurface;

  private:
    // T1 owners, each popup has to have one of these
    CWindow*       m_pWindowOwner = nullptr;
    SLayerSurface* m_pLayerOwner  = nullptr;

    // T2 owners
    CPopup*        m_pParent = nullptr;

    wlr_xdg_popup* m_pWLR = nullptr;

    Vector2D       m_vLastSize = {};
    Vector2D       m_vLastPos  = {};

    bool           m_bRequestedReposition = false;

    bool           m_bInert = false;

    //
    std::vector<std::unique_ptr<CPopup>> m_vChildren;
    std::unique_ptr<CSubsurface>         m_pSubsurfaceHead;

    // signals
    DYNLISTENER(newPopup);
    DYNLISTENER(destroyPopup);
    DYNLISTENER(mapPopup);
    DYNLISTENER(unmapPopup);
    DYNLISTENER(commitPopup);
    DYNLISTENER(repositionPopup);

    void     initAllSignals();
    void     unconstrain();
    void     recheckChildrenRecursive();
    void     sendScale();

    Vector2D localToGlobal(const Vector2D& rel);
    Vector2D t1ParentCoords();
};