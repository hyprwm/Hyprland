#pragma once

#include "../../../helpers/math/Math.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../helpers/signal/Signal.hpp"

class CWLSurfaceResource;

namespace Desktop::View {
    class IPopupBackend {
      public:
        virtual ~IPopupBackend();

        virtual bool                   valid() const           = 0;
        virtual bool                   mapped() const          = 0;
        virtual bool                   initialCommit() const   = 0;
        virtual SP<CWLSurfaceResource> surface() const         = 0;
        virtual CBox                   popupGeometry() const   = 0;
        virtual CBox                   surfaceGeometry() const = 0;
        virtual Vector2D               surfaceSize() const     = 0;

        virtual void                   applyPositioning(const CBox& availableBox, const Vector2D& t1Coord) = 0;
        virtual void                   scheduleConfigure()                                                 = 0;

        struct {
            CSignalT<>                  reposition;
            CSignalT<>                  map;
            CSignalT<>                  unmap;
            CSignalT<>                  dismissed;
            CSignalT<>                  destroy;
            CSignalT<>                  commit;
            CSignalT<SP<IPopupBackend>> newPopup;
        } m_events;

        IPopupBackend(const IPopupBackend&)            = delete;
        IPopupBackend(IPopupBackend&&)                 = delete;
        IPopupBackend& operator=(const IPopupBackend&) = delete;
        IPopupBackend& operator=(IPopupBackend&&)      = delete;

      protected:
        IPopupBackend();
    };
}
