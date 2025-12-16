#pragma once

#include <vector>
#include "Subsurface.hpp"
#include "View.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/AnimatedVariable.hpp"

class CXDGPopupResource;

namespace Desktop::View {

    class CPopup : public IView {
      public:
        // dummy head nodes
        static SP<CPopup> create(PHLWINDOW pOwner);
        static SP<CPopup> create(PHLLS pOwner);

        // real nodes
        static SP<CPopup> create(SP<CXDGPopupResource> popup, WP<CPopup> pOwner);

        static SP<CPopup> fromView(SP<IView>);

        virtual ~CPopup();

        virtual eViewType             type() const;
        virtual bool                  visible() const;
        virtual std::optional<CBox>   logicalBox() const;
        virtual bool                  desktopComponent() const;
        virtual std::optional<CBox>   surfaceLogicalBox() const;

        SP<Desktop::View::CWLSurface> getT1Owner() const;
        Vector2D                      coordsRelativeToParent() const;
        Vector2D                      coordsGlobal() const;
        PHLMONITOR                    getMonitor() const;

        Vector2D                      size() const;

        void                          onNewPopup(SP<CXDGPopupResource> popup);
        void                          onDestroy();
        void                          onMap();
        void                          onUnmap();
        void                          onCommit(bool ignoreSiblings = false);
        void                          onReposition();

        void                          recheckTree();

        bool                          inert() const;

        // will also loop over this node
        void                      breadthfirst(std::function<void(SP<Desktop::View::CPopup>, void*)> fn, void* data);
        SP<Desktop::View::CPopup> at(const Vector2D& globalCoords, bool allowsInput = false);

        //
        WP<Desktop::View::CPopup> m_self;
        bool                      m_mapped = false;

        // fade in-out
        PHLANIMVAR<float> m_alpha;
        bool              m_fadingOut = false;

      private:
        CPopup();

        // T1 owners, each popup has to have one of these
        PHLWINDOWREF m_windowOwner;
        PHLLSREF     m_layerOwner;

        // T2 owners
        WP<Desktop::View::CPopup> m_parent;

        WP<CXDGPopupResource>     m_resource;

        Vector2D                  m_lastSize = {};
        Vector2D                  m_lastPos  = {};

        bool                      m_requestedReposition = false;

        bool                      m_inert = false;

        //
        std::vector<SP<Desktop::View::CPopup>> m_children;
        SP<Desktop::View::CSubsurface>         m_subsurfaceHead;

        struct {
            CHyprSignalListener newPopup;
            CHyprSignalListener destroy;
            CHyprSignalListener map;
            CHyprSignalListener unmap;
            CHyprSignalListener commit;
            CHyprSignalListener dismissed;
            CHyprSignalListener reposition;
        } m_listeners;

        void        initAllSignals();
        void        reposition();
        void        recheckChildrenRecursive();
        void        sendScale();
        void        fullyDestroy();

        Vector2D    localToGlobal(const Vector2D& rel) const;
        Vector2D    t1ParentCoords() const;
        static void bfHelper(std::vector<SP<CPopup>> const& nodes, std::function<void(SP<CPopup>, void*)> fn, void* data);
    };
}
