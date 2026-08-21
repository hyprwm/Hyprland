#pragma once

#include "../DesktopTypes.hpp"
#include "../../helpers/math/Math.hpp"

#include <vector>

class CWLSurfaceResource;

namespace Desktop {
    class IViewStateTracker;

    class CViewHitTester {
      public:
        CViewHitTester(const IViewStateTracker& tracker);
        ~CViewHitTester() = default;

        CViewHitTester(const CViewHitTester&) = delete;
        CViewHitTester(CViewHitTester&)       = delete;
        CViewHitTester(CViewHitTester&&)      = delete;

        PHLWINDOW              windowAt(const Vector2D& pos, uint16_t properties, PHLWINDOW ignoreWindow = nullptr) const;
        SP<CWLSurfaceResource> windowSurfaceAt(const Vector2D& pos, PHLWINDOW window, Vector2D& surfaceLocal) const;
        Vector2D               surfaceLocalAt(const Vector2D& pos, PHLWINDOW window, SP<CWLSurfaceResource> surface) const;
        SP<CWLSurfaceResource> layerPopupSurfaceAt(const Vector2D& pos, PHLMONITOR monitor, Vector2D* surfaceCoords, PHLLS* layerFound) const;
        SP<CWLSurfaceResource> layerSurfaceAt(const Vector2D& pos, std::vector<PHLLSREF>* layerSurfaces, Vector2D* surfaceCoords, PHLLS* layerFound,
                                              bool aboveLockscreen = false) const;

      private:
        const IViewStateTracker& m_tracker;
    };
}
