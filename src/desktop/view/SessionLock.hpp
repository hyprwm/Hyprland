#pragma once

#include "../../defines.hpp"
#include <vector>
#include "WLSurface.hpp"
#include "View.hpp"
#include "types/Geometric.hpp"

class CSessionLockSurface;

namespace Desktop::View {
    class CSessionLock : public IView, public virtual IGeometric {
      public:
        static SP<CSessionLock> create(SP<CSessionLockSurface> resource);

        static SP<CSessionLock> fromView(SP<IView>);

        virtual ~CSessionLock();

        virtual eViewType           type() const;
        virtual bool                visible() const;
        virtual std::optional<CBox> logicalBox() const;
        virtual bool                desktopComponent() const;
        virtual std::optional<CBox> surfaceLogicalBox() const;
        virtual Vector2D            position(eGeometricValueType) const override;
        virtual Vector2D            size(eGeometricValueType) const override;
        virtual CBox                geometricBox(eGeometricValueType) const override;

        PHLMONITOR                  monitor() const;

        WP<CSessionLock>            m_self;

      private:
        CSessionLock();

        void                    init();

        WP<CSessionLockSurface> m_surface;
    };
}
