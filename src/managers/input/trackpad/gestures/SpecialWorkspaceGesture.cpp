#include "SpecialWorkspaceGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../managers/LayoutManager.hpp"
#include "../../../../render/Renderer.hpp"

#include <hyprutils/memory/Casts.hpp>
using namespace Hyprutils::Memory;

constexpr const float MAX_DISTANCE = 150.F;

//
static Vector2D lerpVal(const Vector2D& from, const Vector2D& to, const float& t) {
    return Vector2D{
        from.x + ((to.x - from.x) * t),
        from.y + ((to.y - from.y) * t),
    };
}

static float lerpVal(const float& from, const float& to, const float& t) {
    return from + ((to - from) * t);
}

CSpecialWorkspaceGesture::CSpecialWorkspaceGesture(const std::string& workspaceName) : m_specialWorkspaceName(workspaceName) {
    ;
}

void CSpecialWorkspaceGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_specialWorkspace.reset();
    m_lastDelta = 0.F;
    m_monitor.reset();

    m_specialWorkspace = g_pCompositor->getWorkspaceByName("special:" + m_specialWorkspaceName);

    if (m_specialWorkspace) {
        m_animatingOut = m_specialWorkspace->isVisible();
        m_monitor      = m_animatingOut ? m_specialWorkspace->m_monitor : g_pCompositor->m_lastMonitor;

        if (!m_monitor)
            return;

        if (!m_animatingOut)
            m_monitor->setSpecialWorkspace(m_specialWorkspace);
    } else {
        m_monitor = g_pCompositor->m_lastMonitor;

        if (!m_monitor)
            return;

        m_animatingOut = false;

        const auto& [workspaceID, workspaceName] = getWorkspaceIDNameFromString("special:" + m_specialWorkspaceName);
        const auto WS                            = g_pCompositor->createNewWorkspace(workspaceID, m_monitor->m_id, workspaceName);
        m_monitor->setSpecialWorkspace(WS);
        m_specialWorkspace = WS;
    }

    if (!m_specialWorkspace)
        return;

    m_monitorDimFrom      = m_monitor->m_specialFade->begun();
    m_monitorDimTo        = m_monitor->m_specialFade->goal();
    m_workspaceAlphaFrom  = m_specialWorkspace->m_alpha->begun();
    m_workspaceAlphaTo    = m_specialWorkspace->m_alpha->goal();
    m_workspaceOffsetFrom = m_specialWorkspace->m_renderOffset->begun();
    m_workspaceOffsetTo   = m_specialWorkspace->m_renderOffset->goal();
}

void CSpecialWorkspaceGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_specialWorkspace || !m_monitor)
        return;

    g_pHyprRenderer->damageMonitor(m_specialWorkspace->m_monitor.lock());

    m_lastDelta += distance(e);

    const auto FADEPERCENT = m_animatingOut ? 1.F - std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F) : std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    m_monitor->m_specialFade->setValueAndWarp(lerpVal(m_monitorDimFrom, m_monitorDimTo, FADEPERCENT));
    m_specialWorkspace->m_alpha->setValueAndWarp(lerpVal(m_workspaceAlphaFrom, m_workspaceAlphaTo, FADEPERCENT));
    m_specialWorkspace->m_renderOffset->setValueAndWarp(lerpVal(m_workspaceOffsetFrom, m_workspaceOffsetTo, FADEPERCENT));
}

void CSpecialWorkspaceGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!m_specialWorkspace || !m_monitor)
        return;

    const auto COMPLETION = std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    if (COMPLETION < 0.3F) {
        // cancel the operation, which effectively means just flip the animation direction
        // also flip goals if animating in
        m_animatingOut = !m_animatingOut;

        if (m_animatingOut) {
            m_workspaceOffsetTo = m_workspaceOffsetFrom;
            m_workspaceAlphaTo  = m_workspaceAlphaFrom;
            m_monitorDimTo      = m_monitorDimFrom;
        }
    }

    if (m_animatingOut) {
        const auto CURR_WS_ALPHA  = m_specialWorkspace->m_alpha->value();
        const auto CURR_WS_OFFSET = m_specialWorkspace->m_renderOffset->value();
        const auto CURR_MON_FADE  = m_monitor->m_specialFade->value();

        m_monitor->setSpecialWorkspace(nullptr);

        const auto GOAL_WS_ALPHA  = m_specialWorkspace->m_alpha->goal();
        const auto GOAL_WS_OFFSET = m_specialWorkspace->m_renderOffset->goal();

        m_monitor->m_specialFade->setValueAndWarp(CURR_MON_FADE);
        m_specialWorkspace->m_alpha->setValueAndWarp(CURR_WS_ALPHA);
        m_specialWorkspace->m_renderOffset->setValueAndWarp(CURR_WS_OFFSET);

        *m_monitor->m_specialFade           = 0.F;
        *m_specialWorkspace->m_alpha        = GOAL_WS_ALPHA;
        *m_specialWorkspace->m_renderOffset = GOAL_WS_OFFSET;
    } else {
        *m_monitor->m_specialFade           = m_monitorDimTo;
        *m_specialWorkspace->m_renderOffset = m_workspaceOffsetTo;
        *m_specialWorkspace->m_alpha        = m_workspaceAlphaTo;
    }
}
