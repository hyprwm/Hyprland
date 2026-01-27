#pragma once

#include "../../defines.hpp"
#include <vector>
#include "WLSurface.hpp"
#include "View.hpp"

class CSessionLockSurface;

namespace Desktop::View {
    class CSessionLock : public IView {
      public:
        static SP<CSessionLock> create(SP<CSessionLockSurface> resource);

        static SP<CSessionLock> fromView(SP<IView>);

        virtual ~CSessionLock();

        virtual eViewType           type() const;
        virtual bool                visible() const;
        virtual std::optional<CBox> logicalBox() const;
        virtual bool                desktopComponent() const;
        virtual std::optional<CBox> surfaceLogicalBox() const;

        PHLMONITOR                  monitor() const;

        WP<CSessionLock>            m_self;

      private:
        CSessionLock();

        void init();

        struct {
            CHyprSignalListener destroy;
        } m_listeners;

        WP<CSessionLockSurface> m_surface;
    };
}
