#include "SessionLock.hpp"

#include "../../protocols/SessionLock.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../helpers/Monitor.hpp"

#include "../../Compositor.hpp"

using namespace Desktop;
using namespace Desktop::View;

SP<View::CSessionLock> View::CSessionLock::create(SP<CSessionLockSurface> resource) {
    auto lock       = SP<CSessionLock>(new CSessionLock());
    lock->m_surface = resource;
    lock->m_self    = lock;

    lock->init();

    return lock;
}

View::CSessionLock::CSessionLock() : IView(CWLSurface::create()) {
    ;
}

View::CSessionLock::~CSessionLock() {
    m_wlSurface->unassign();
}

void View::CSessionLock::init() {
    m_listeners.destroy = m_surface->m_events.destroy.listen([this] { std::erase_if(g_pCompositor->m_otherViews, [this](const auto& e) { return e == m_self; }); });

    m_wlSurface->assign(m_surface->surface(), m_self.lock());
}

SP<View::CSessionLock> View::CSessionLock::fromView(SP<IView> v) {
    if (!v || v->type() != VIEW_TYPE_LOCK_SCREEN)
        return nullptr;
    return dynamicPointerCast<View::CSessionLock>(v);
}

eViewType View::CSessionLock::type() const {
    return VIEW_TYPE_LOCK_SCREEN;
}

bool View::CSessionLock::visible() const {
    return m_wlSurface && m_wlSurface->resource() && m_wlSurface->resource()->m_mapped;
}

std::optional<CBox> View::CSessionLock::logicalBox() const {
    return surfaceLogicalBox();
}

std::optional<CBox> View::CSessionLock::surfaceLogicalBox() const {
    if (!visible())
        return std::nullopt;

    const auto MON = m_surface->monitor();

    if (!MON)
        return std::nullopt;

    return MON->logicalBox();
}

bool View::CSessionLock::desktopComponent() const {
    return true;
}

PHLMONITOR View::CSessionLock::monitor() const {
    if (m_surface)
        return m_surface->monitor();
    return nullptr;
}
