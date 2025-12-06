#include "View.hpp"

using namespace Desktop;
using namespace Desktop::View;

SP<Desktop::View::CWLSurface> IView::wlSurface() const {
    return m_wlSurface;
}

IView::IView(SP<Desktop::View::CWLSurface> pWlSurface) : m_wlSurface(pWlSurface) {
    ;
}

SP<CWLSurfaceResource> IView::resource() const {
    return m_wlSurface ? m_wlSurface->resource() : nullptr;
}
