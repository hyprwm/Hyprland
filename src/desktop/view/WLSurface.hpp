#pragma once

#include "../../defines.hpp"
#include "../../helpers/math/Math.hpp"
#include "../../helpers/signal/Signal.hpp"

class CPointerConstraint;
class CWLSurfaceResource;

namespace Desktop::View {
    class CSubsurface;
    class CPopup;
    class IView;

    class CWLSurface {
      public:
        static SP<Desktop::View::CWLSurface> create() {
            auto p    = SP<Desktop::View::CWLSurface>(new CWLSurface);
            p->m_self = p;
            return p;
        }
        ~CWLSurface();

        // anonymous surfaces are non-desktop components, e.g. a cursor surface or a DnD
        void assign(SP<CWLSurfaceResource> pSurface);
        void assign(SP<CWLSurfaceResource> pSurface, SP<IView> pOwner);
        void unassign();

        CWLSurface(const CWLSurface&)                       = delete;
        CWLSurface(CWLSurface&&)                            = delete;
        CWLSurface&            operator=(const CWLSurface&) = delete;
        CWLSurface&            operator=(CWLSurface&&)      = delete;

        SP<CWLSurfaceResource> resource() const;
        bool                   exists() const;
        bool                   small() const;              // means surface is smaller than the requested size
        Vector2D               correctSmallVec() const;    // returns a corrective vector for small() surfaces
        Vector2D               correctSmallVecBuf() const; // returns a corrective vector for small() surfaces, in BL coords
        Vector2D               getViewporterCorrectedSize() const;
        CRegion                computeDamage() const; // logical coordinates. May be wrong if the surface is unassigned
        bool                   keyboardFocusable() const;

        SP<IView>              view() const;

        // desktop components misc utils
        std::optional<CBox>    getSurfaceBoxGlobal() const;
        void                   appendConstraint(WP<CPointerConstraint> constraint);
        SP<CPointerConstraint> constraint() const;

        // allow stretching. Useful for plugins.
        bool m_fillIgnoreSmall = false;

        // track surface data and avoid dupes
        float               m_lastScaleFloat = 0;
        int                 m_lastScaleInt   = 0;
        wl_output_transform m_lastTransform  = sc<wl_output_transform>(-1);

        //
        CWLSurface& operator=(SP<CWLSurfaceResource> pSurface) {
            destroy();
            m_resource = pSurface;
            init();

            return *this;
        }

        bool operator==(const CWLSurface& other) const {
            return other.resource() == resource();
        }

        bool operator==(const SP<CWLSurfaceResource> other) const {
            return other == resource();
        }

        explicit operator bool() const {
            return exists();
        }

        static SP<Desktop::View::CWLSurface> fromResource(SP<CWLSurfaceResource> pSurface);

        // used by the alpha-modifier protocol
        float m_alphaModifier = 1.F;

        // used by the hyprland-surface protocol
        float   m_overallOpacity = 1.F;
        CRegion m_visibleRegion;

        struct {
            CSignalT<> destroy;
        } m_events;

        WP<Desktop::View::CWLSurface> m_self;

      private:
        CWLSurface() = default;

        bool                   m_inert = true;

        WP<CWLSurfaceResource> m_resource;

        WP<IView>              m_view;

        //
        WP<CPointerConstraint> m_constraint;

        void                   destroy();
        void                   init();
        bool                   desktopComponent() const;

        struct {
            CHyprSignalListener destroy;
        } m_listeners;

        friend class ::CPointerConstraint;
    };
}
