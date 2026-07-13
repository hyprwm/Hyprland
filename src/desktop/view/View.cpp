#include "View.hpp"
#include "../../event/EventBus.hpp"
#include "../../protocols/core/Compositor.hpp"

using namespace Desktop;
using namespace Desktop::View;

SP<Desktop::View::CWLSurface> IView::wlSurface() const {
    return m_wlSurface;
}

IView::IView(SP<Desktop::View::CWLSurface> pWlSurface) : m_wlSurface(pWlSurface) {
    ;
}

IView::~IView() {
    if (!m_initialized)
        return;

    Event::bus()->m_events.view.destroy.emit({.view = m_self, .type = m_type, .address = m_address});
}

void IView::initView(WP<IView> self, eViewType type) {
    const auto VIEW = self.lock();

    if (!VIEW)
        return;

    m_self        = self;
    m_type        = type;
    m_address     = rc<uintptr_t>(VIEW.get());
    m_initialized = true;

    Event::bus()->m_events.view.create.emit(VIEW);
}

SP<CWLSurfaceResource> IView::resource() const {
    return m_wlSurface ? m_wlSurface->resource() : nullptr;
}

bool IView::aliveAndVisible() const {
    auto res = resource();
    if (!res)
        return false;

    if (!res->m_mapped)
        return false;

    return visible();
}
