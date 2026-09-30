#include "MonitorScene.hpp"

#include "../Renderer.hpp"
#include "../../event/EventBus.hpp"
#include "../../output/Monitor.hpp"

using namespace Render;

CMonitorScene::CMonitorScene(PHLMONITORREF monitor) : m_monitor(monitor) {
    ;
}

void CMonitorScene::draw(const Time::steady_tp& now) {
    const auto MONITOR = m_monitor.lock();
    if (!MONITOR)
        return;

    if (MONITOR->isMirror()) {
        g_pHyprRenderer->blend(false);
        g_pHyprRenderer->renderMirrored();
        g_pHyprRenderer->blend(true);
        Event::bus()->m_events.render.stage.emit(RENDER_POST_MIRROR);
        return;
    }

    const CBox GEOMETRY = {0, 0, sc<int>(MONITOR->m_transformedSize.x), sc<int>(MONITOR->m_transformedSize.y)};
    g_pHyprRenderer->renderWorkspace(MONITOR, MONITOR->m_activeWorkspace, now, GEOMETRY);
    g_pHyprRenderer->renderLockscreen(MONITOR, now, GEOMETRY);

    // Render IME above the lockscreen so it can be used to unlock the session.
    g_pHyprRenderer->renderIME(MONITOR, now, GEOMETRY);
}
