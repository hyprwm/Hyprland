#pragma once

#include <span>
#include <vector>
#include <cstdint>
#include "Subsurface.hpp"
#include "View.hpp"
#include "types/Geometric.hpp"
#include "types/AlphaModifiable.hpp"
#include "animationControllers/PopupAnimationController.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../render/Framebuffer.hpp"

class CXDGPopupResource;

namespace Desktop::View {

    enum ePopupAlpha : uint8_t {
        POPUP_ALPHA_FADE = 0,

        POPUP_ALPHA_LAST,
    };

    class CPopup : public virtual IView, public virtual IGeometric, public virtual IAlphaModifiable {
      public:
        // dummy head nodes
        static SP<CPopup> create(PHLWINDOW pOwner);
        static SP<CPopup> create(PHLLS pOwner);

        // real nodes
        static SP<CPopup> create(SP<CXDGPopupResource> popup, WP<CPopup> pOwner);

        static SP<CPopup> fromView(SP<IView>);

        virtual ~CPopup();

        virtual eViewType                                   type() const override;
        virtual bool                                        visible() const override;
        virtual std::optional<CBox>                         logicalBox() const override;
        virtual bool                                        desktopComponent() const override;
        virtual std::optional<CBox>                         surfaceLogicalBox() const override;
        virtual Vector2D                                    position(eGeometricValueType) const override;
        virtual Vector2D                                    size(eGeometricValueType) const override;
        virtual CBox                                        geometricBox(eGeometricValueType) const override;
        virtual Types::CMultiAVarContainer<float, uint8_t>& alpha() override;
        virtual std::optional<uint8_t>                      alphaGenericToKey(eAlphaModifiableProp p) override;

        SP<Desktop::View::CWLSurface>                       getT1Owner() const;
        PHLLS                                               layerOwner() const;
        Vector2D                                            coordsRelativeToParent() const;
        Vector2D                                            coordsGlobal() const;
        PHLMONITOR                                          getMonitor() const;

        Vector2D                                            size() const;

        void                                                onNewPopup(SP<CXDGPopupResource> popup);
        void                                                onDestroy();
        void                                                onMap();
        void                                                onUnmap();
        void                                                onCommit(bool ignoreSiblings = false);
        void                                                onReposition();

        void                                                recheckTree();

        bool                                                inert() const;

        // will also loop over this node
        void                      breadthfirst(std::function<void(SP<Desktop::View::CPopup>, void*)> fn, void* data);
        SP<Desktop::View::CPopup> at(const Vector2D& globalCoords, bool allowsInput = false);
        SP<Desktop::View::CPopup> popupHead() const;
        const CBox&               popupTreeExtents() const;
        int                       popupTreeCount() const;

        //
        WP<Desktop::View::CPopup> m_self;
        bool                      m_mapped = false;

        CPopupAnimationController m_animationController;

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

        mutable CBox              m_cachedTreeExtents        = {};
        mutable bool              m_treeExtentsCacheDirty    = true;
        mutable int               m_cachedTreePopupCount     = 0;
        mutable bool              m_treePopupCountCacheDirty = true;

        // fade in/out
        Desktop::Types::CMultiAVarContainer<float, std::underlying_type_t<ePopupAlpha>> m_alpha;

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
        void        invalidateTreeExtentsCache();
        static void bfHelper(std::span<const SP<CPopup>> nodes, std::function<void(SP<CPopup>, void*)> fn, void* data);
    };
}
