#pragma once

#include "PopupBackend.hpp"

class CXDGPopupResource;

namespace Desktop::View {
    class CWaylandPopupBackend final : public IPopupBackend {
      public:
        CWaylandPopupBackend(SP<CXDGPopupResource> resource);

        bool                   valid() const override;
        bool                   mapped() const override;
        bool                   initialCommit() const override;
        SP<CWLSurfaceResource> surface() const override;
        CBox                   popupGeometry() const override;
        CBox                   surfaceGeometry() const override;
        Vector2D               surfaceSize() const override;

        void                   applyPositioning(const CBox& availableBox, const Vector2D& t1Coord) override;
        void                   scheduleConfigure() override;

      private:
        WP<CXDGPopupResource>  m_resource;
        WP<CWLSurfaceResource> m_surface;

        struct {
            CHyprSignalListener reposition;
            CHyprSignalListener map;
            CHyprSignalListener unmap;
            CHyprSignalListener dismissed;
            CHyprSignalListener destroy;
            CHyprSignalListener commit;
            CHyprSignalListener newPopup;
        } m_listeners;
    };

    SP<IPopupBackend> makeWaylandPopupBackend(SP<CXDGPopupResource> resource);
}
