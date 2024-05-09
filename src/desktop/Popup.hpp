#pragma once

#include <vector>
#include <memory>
#include "Subsurface.hpp"
#include "../helpers/signal/Listener.hpp"

class CPopup {
  public:
    // dummy head nodes
    CPopup(PHLWINDOW pOwner);
    CPopup(PHLLS pOwner);

    // real nodes
    CPopup(wlr_xdg_popup* popup, CPopup* pOwner);

    ~CPopup();

    Vector2D coordsRelativeToParent();
    Vector2D coordsGlobal();

    Vector2D size();

    void     onNewPopup(wlr_xdg_popup* popup);
    void     onDestroy();
    void     onMap();
    void     onUnmap();
    void     onCommit(bool ignoreSiblings = false);
    void     onReposition();

    void     recheckTree();

    bool     visible();

    // will also loop over this node
    void    breadthfirst(std::function<void(CPopup*, void*)> fn, void* data);
    CPopup* at(const Vector2D& globalCoords, bool allowsInput = false);

    //
    CWLSurface m_sWLSurface;

  private:
    // T1 owners, each popup has to have one of these
    PHLWINDOWREF m_pWindowOwner;
    PHLLSREF     m_pLayerOwner;

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

    struct {
        CHyprSignalListener newPopup;
    } listeners;

    void        initAllSignals();
    void        unconstrain();
    void        recheckChildrenRecursive();
    void        sendScale();

    Vector2D    localToGlobal(const Vector2D& rel);
    Vector2D    t1ParentCoords();
    static void bfHelper(std::vector<CPopup*> nodes, std::function<void(CPopup*, void*)> fn, void* data);
};