#pragma once

#include <vector>
#include "Subsurface.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/memory/Memory.hpp"

class CXDGPopupResource;

class CPopup {
  public:
    // dummy head nodes
    static UP<CPopup> create(PHLWINDOW pOwner);
    static UP<CPopup> create(PHLLS pOwner);

    // real nodes
    static UP<CPopup> create(SP<CXDGPopupResource> popup, WP<CPopup> pOwner);

    ~CPopup();

    SP<CWLSurface> getT1Owner();
    Vector2D       coordsRelativeToParent();
    Vector2D       coordsGlobal();

    Vector2D       size();

    void           onNewPopup(SP<CXDGPopupResource> popup);
    void           onDestroy();
    void           onMap();
    void           onUnmap();
    void           onCommit(bool ignoreSiblings = false);
    void           onReposition();

    void           recheckTree();

    bool           visible();
    bool           inert() const {
        return m_bInert;
    }

    // will also loop over this node
    void       breadthfirst(std::function<void(WP<CPopup>, void*)> fn, void* data);
    WP<CPopup> at(const Vector2D& globalCoords, bool allowsInput = false);

    //
    SP<CWLSurface> m_pWLSurface;
    WP<CPopup>     m_pSelf;
    bool           m_bMapped = false;

  private:
    CPopup() = default;

    // T1 owners, each popup has to have one of these
    PHLWINDOWREF m_pWindowOwner;
    PHLLSREF     m_pLayerOwner;

    // T2 owners
    WP<CPopup>            m_pParent;

    WP<CXDGPopupResource> m_pResource;

    Vector2D              m_vLastSize = {};
    Vector2D              m_vLastPos  = {};

    bool                  m_bRequestedReposition = false;

    bool                  m_bInert = false;

    //
    std::vector<UP<CPopup>> m_vChildren;
    UP<CSubsurface>         m_pSubsurfaceHead;

    struct {
        CHyprSignalListener newPopup;
        CHyprSignalListener destroy;
        CHyprSignalListener map;
        CHyprSignalListener unmap;
        CHyprSignalListener commit;
        CHyprSignalListener dismissed;
        CHyprSignalListener reposition;
    } listeners;

    void        initAllSignals();
    void        reposition();
    void        recheckChildrenRecursive();
    void        sendScale();

    Vector2D    localToGlobal(const Vector2D& rel);
    Vector2D    t1ParentCoords();
    static void bfHelper(std::vector<WP<CPopup>> const& nodes, std::function<void(WP<CPopup>, void*)> fn, void* data);
};
