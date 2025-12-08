#pragma once

#include "../../defines.hpp"
#include <vector>
#include "WLSurface.hpp"
#include "View.hpp"

class CWLSubsurfaceResource;

namespace Desktop::View {
    class CPopup;
    class CSubsurface : public IView {
      public:
        // root dummy nodes
        static SP<CSubsurface> create(PHLWINDOW pOwner);
        static SP<CSubsurface> create(WP<Desktop::View::CPopup> pOwner);

        // real nodes
        static SP<CSubsurface> create(SP<CWLSubsurfaceResource> pSubsurface, PHLWINDOW pOwner);
        static SP<CSubsurface> create(SP<CWLSubsurfaceResource> pSubsurface, WP<Desktop::View::CPopup> pOwner);

        static SP<CSubsurface> fromView(SP<IView>);

        virtual ~CSubsurface() = default;

        virtual eViewType              type() const;
        virtual bool                   visible() const;
        virtual std::optional<CBox>    logicalBox() const;
        virtual bool                   desktopComponent() const;
        virtual std::optional<CBox>    surfaceLogicalBox() const;

        Vector2D                       coordsRelativeToParent() const;
        Vector2D                       coordsGlobal() const;

        Vector2D                       size();

        void                           onCommit();
        void                           onDestroy();
        void                           onNewSubsurface(SP<CWLSubsurfaceResource> pSubsurface);
        void                           onMap();
        void                           onUnmap();

        void                           recheckDamageForSubsurfaces();

        WP<Desktop::View::CSubsurface> m_self;

      private:
        CSubsurface();

        struct {
            CHyprSignalListener destroySubsurface;
            CHyprSignalListener commitSubsurface;
            CHyprSignalListener mapSubsurface;
            CHyprSignalListener unmapSubsurface;
            CHyprSignalListener newSubsurface;
        } m_listeners;

        WP<CWLSubsurfaceResource> m_subsurface;
        Vector2D                  m_lastSize     = {};
        Vector2D                  m_lastPosition = {};

        // if nullptr, means it's a dummy node
        WP<Desktop::View::CSubsurface>              m_parent;

        PHLWINDOWREF                                m_windowParent;
        WP<Desktop::View::CPopup>                   m_popupParent;

        std::vector<SP<Desktop::View::CSubsurface>> m_children;

        bool                                        m_inert = false;

        void                                        initSignals();
        void                                        initExistingSubsurfaces(SP<CWLSurfaceResource> pSurface);
        void                                        checkSiblingDamage();
        void                                        damageLastArea();
    };
}
