#pragma once

#include "WLSurface.hpp"
#include "../../helpers/math/Math.hpp"

namespace Desktop::View {
    enum eViewType : uint8_t {
        VIEW_TYPE_WINDOW = 0,
        VIEW_TYPE_SUBSURFACE,
        VIEW_TYPE_POPUP,
        VIEW_TYPE_LAYER_SURFACE,
        VIEW_TYPE_LOCK_SCREEN,
    };

    class IView {
      public:
        virtual ~IView() = default;

        virtual SP<Desktop::View::CWLSurface> wlSurface() const;
        virtual SP<CWLSurfaceResource>        resource() const;
        virtual bool                          aliveAndVisible() const;
        virtual eViewType                     type() const              = 0;
        virtual bool                          visible() const           = 0;
        virtual bool                          desktopComponent() const  = 0;
        virtual std::optional<CBox>           logicalBox() const        = 0;
        virtual std::optional<CBox>           surfaceLogicalBox() const = 0;

      protected:
        IView(SP<Desktop::View::CWLSurface> pWlSurface);

        SP<Desktop::View::CWLSurface> m_wlSurface;
    };
};