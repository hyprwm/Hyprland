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
        static UP<Desktop::View::CSubsurface> create(PHLWINDOW pOwner);
        static UP<Desktop::View::CSubsurface> create(WP<Desktop::View::CPopup> pOwner);

        // real nodes
        static UP<Desktop::View::CSubsurface> create(SP<CWLSubsurfaceResource> pSubsurface, PHLWINDOW pOwner);
        static UP<Desktop::View::CSubsurface> create(SP<CWLSubsurfaceResource> pSubsurface, WP<Desktop::View::CPopup> pOwner);

        virtual ~CSubsurface() = default;

        virtual eViewType              type() const;
        virtual bool                   visible() const;
        virtual std::optional<CBox>    logicalBox() const;

        Vector2D                       coordsRelativeToParent();
        Vector2D                       coordsGlobal();

        Vector2D                       size();

        void                           onCommit();
        void                           onDestroy();
        void                           onNewSubsurface(SP<CWLSubsurfaceResource> pSubsurface);
        void                           onMap();
        void                           onUnmap();

        bool                           visible();

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

        std::vector<UP<Desktop::View::CSubsurface>> m_children;

        bool                                        m_inert = false;

        void                                        initSignals();
        void                                        initExistingSubsurfaces(SP<CWLSurfaceResource> pSurface);
        void                                        checkSiblingDamage();
        void                                        damageLastArea();
    };
}
