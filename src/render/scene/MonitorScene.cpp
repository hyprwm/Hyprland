#include "MonitorScene.hpp"

#include "../../output/Monitor.hpp"
#include "../../state/MonitorState.hpp"
#include "../../event/EventBus.hpp"

#include "../../macros.hpp"
#include "../Renderer.hpp"

using namespace Render;

CMonitorScene::CMonitorScene(PHLMONITOR mon) : m_monitor(mon) {
    ;
}

void CMonitorScene::draw(CRenderingContext& context, Time::steady_tp tp) {
    RASSERT(m_monitor, "Attemted to CMonitorScene::draw() a null monitor");

    if (m_monitor->isMirror()) {
        g_pHyprRenderer->blend(false);
        g_pHyprRenderer->renderMirrored(context);
        g_pHyprRenderer->blend(true);
        Event::bus()->m_events.render.stage.emit(RENDER_POST_MIRROR);
    } else {
        CBox renderBox = {0, 0, sc<int>(m_monitor->m_transformedSize.x), sc<int>(m_monitor->m_transformedSize.y)};
        g_pHyprRenderer->renderWorkspace(context, m_monitor.lock(), m_monitor->m_activeWorkspace, tp, renderBox);
        g_pHyprRenderer->renderLockscreen(context, m_monitor.lock(), tp, renderBox);

        // render IME even above the lockscreen - allow the user to use it to potentially input stuff on it.
        g_pHyprRenderer->renderIME(context, m_monitor.lock(), tp, renderBox);
    }
}
