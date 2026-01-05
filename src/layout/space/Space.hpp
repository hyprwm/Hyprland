#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "../../desktop/DesktopTypes.hpp"

namespace Layout {
    class ITarget;
    class CAlgorithm;

    class CSpace {
      public:
        static SP<CSpace> create(PHLWORKSPACE w);
        ~CSpace() = default;

        void        add(SP<ITarget> t);
        void        remove(SP<ITarget> t);

        void        setAlgorithmProvider(SP<CAlgorithm> algo);
        void        recheckWorkArea();

        const CBox& workArea() const;

      private:
        CSpace(PHLWORKSPACE parent);

        WP<CSpace>               m_self;

        std::vector<SP<ITarget>> m_targets;
        SP<CAlgorithm>           m_algorithm;
        PHLWORKSPACEREF          m_parent;

        // work area is in global coords
        CBox m_workArea;
    };
};