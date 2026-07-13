#pragma once

#include "../DesktopTypes.hpp"
#include "../view/View.hpp"

#include <optional>
#include <string>
#include <string_view>

class CWLSurfaceResource;

namespace Desktop {
    class IViewStateTracker;

    class CViewQuery {
      public:
        CViewQuery(const IViewStateTracker& tracker);
        ~CViewQuery() = default;

        CViewQuery(const CViewQuery&) = delete;
        CViewQuery(CViewQuery&)       = delete;
        CViewQuery(CViewQuery&&)      = delete;

        CViewQuery&& type(View::eViewType type) &&;
        CViewQuery&& surface(SP<CWLSurfaceResource> surface) &&;
        CViewQuery&& handle(uint32_t handle) &&;
        CViewQuery&& selector(std::string_view selector) &&;
        CViewQuery&& urgent() &&;
        CViewQuery&& forceFocus() &&;
        CViewQuery&& mappedOnly(bool mappedOnly = true) &&;

        PHLVIEW      run() &&;
        PHLWINDOW    runWindow() &&;
        PHLLS        runLayer() &&;

      private:
        PHLWINDOW                             byHandle() const;
        PHLWINDOW                             bySelector() const;
        PHLWINDOW                             urgentWindow() const;
        PHLWINDOW                             forceFocusWindow() const;
        PHLVIEW                               bySurface() const;
        PHLLS                                 layerBySurface() const;

        bool                                  typeMatches(PHLVIEW view) const;
        bool                                  windowMatchesCommon(PHLWINDOW window) const;

        const IViewStateTracker&              m_tracker;

        std::optional<View::eViewType>        m_type;
        std::optional<SP<CWLSurfaceResource>> m_surface;
        std::optional<uint32_t>               m_handle;
        std::optional<std::string>            m_selector;

        bool                                  m_urgent     = false;
        bool                                  m_forceFocus = false;
        bool                                  m_mappedOnly = false;
    };
}
