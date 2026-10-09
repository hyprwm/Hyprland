#include "SpecialWorkspaceGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../state/WorkspaceState.hpp"
#include "../../../../state/workspace/Resolver.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../render/Renderer.hpp"
#include "../../../../workspace/WorkspaceUtils.hpp"

#include <cmath>

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

CSpecialWorkspaceGesture::CSpecialWorkspaceGesture(const std::string& workspaceName) : m_specialWorkspaceName(workspaceName) {
    ;
}

CSpecialWorkspaceGesture::~CSpecialWorkspaceGesture() {
    cancel();
}

static bool isActiveSpecialOnMonitor(const PHLWORKSPACE& workspace, const PHLMONITOR& monitor) {
    return monitor && monitor->m_enabled && workspace && workspace->m_monitor == monitor && monitor->m_activeSpecialWorkspace == workspace && workspace->visible();
}

bool CSpecialWorkspaceGesture::ownsActiveSpecial() const {
    return isActiveSpecialOnMonitor(m_specialWorkspace, m_monitor.lock());
}

void CSpecialWorkspaceGesture::resetSession() {
    m_sessionActive = false;
    m_listeners.monitorDisconnect.reset();
    m_listeners.monitorDestroy.reset();
    m_listeners.workspaceMonitorChanged.reset();
    m_listeners.workspaceActiveChanged.reset();

    // A preexisting rendering hold belongs to its original caller.
    if (m_forceRenderingAcquired && m_specialWorkspace)
        m_specialWorkspace->m_forceRendering = false;

    m_forceRenderingAcquired = false;
    m_specialWorkspace.reset();
    m_monitor.reset();
    m_animatingOut = false;
    m_lastDelta    = 0.F;
    m_hasPreview   = false;
}

void CSpecialWorkspaceGesture::cancel() {
    // Internal cancellation rolls back the operation regardless of its progress.
    finish(!m_animatingOut);
}

void CSpecialWorkspaceGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    cancel();
    ITrackpadGesture::begin(e);

    const auto SPECIAL_WORKSPACE_ADDRESS = Workspace::specialWorkspaceAddressFromName(m_specialWorkspaceName);
    auto       workspace                 = State::Workspace::state()->query().address(SPECIAL_WORKSPACE_ADDRESS).run();
    const bool ANIMATING_OUT             = workspace && workspace->visible();
    const auto MONITOR                   = ANIMATING_OUT ? workspace->m_monitor.lock() : Desktop::focusState()->monitor();

    if (!MONITOR || !MONITOR->m_enabled)
        return;

    if (!workspace) {
        const auto TARGET = State::Workspace::resolver()->getWorkspaceTargetFromString(SPECIAL_WORKSPACE_ADDRESS);
        workspace         = State::Workspace::state()->create(TARGET, MONITOR);
    }

    if (!workspace)
        return;

    // Publish the session and attach listeners only after our activation has emitted its signals.
    if (!ANIMATING_OUT)
        MONITOR->setSpecialWorkspace(workspace);

    workspace->ready();

    if (!MONITOR->m_enabled || workspace->m_monitor != MONITOR || MONITOR->m_activeSpecialWorkspace != workspace || !workspace->visible())
        return;

    m_specialWorkspace = workspace;
    m_monitor          = MONITOR;
    m_animatingOut     = ANIMATING_OUT;

    m_monitorFadeFrom     = m_monitor->m_specialFade->begun();
    m_monitorFadeTo       = m_monitor->m_specialFade->goal();
    m_monitorDimFrom      = m_monitor->m_specialDim->begun();
    m_monitorDimTo        = m_monitor->m_specialDim->goal();
    m_monitorBlurFrom     = m_monitor->m_specialBlur->begun();
    m_monitorBlurTo       = m_monitor->m_specialBlur->goal();
    m_workspaceAlphaFrom  = m_specialWorkspace->m_alpha->begun();
    m_workspaceAlphaTo    = m_specialWorkspace->m_alpha->goal();
    m_workspaceOffsetFrom = m_specialWorkspace->m_renderOffset->begun();
    m_workspaceOffsetTo   = m_specialWorkspace->m_renderOffset->goal();

    m_forceRenderingAcquired             = !m_specialWorkspace->m_forceRendering;
    m_specialWorkspace->m_forceRendering = true;
    m_sessionActive                      = true;

    const auto VALIDATE = [this] {
        if (!ownsActiveSpecial())
            cancel();
    };
    // Disconnect is emitted mid-teardown, before m_enabled is cleared. Leave activation to monitor migration.
    m_listeners.monitorDisconnect       = MONITOR->m_events.disconnect.listen([this] { resetSession(); });
    m_listeners.monitorDestroy          = MONITOR->m_events.destroy.listen([this] { resetSession(); });
    m_listeners.workspaceMonitorChanged = m_specialWorkspace->m_events.monitorChanged.listen(VALIDATE);
    m_listeners.workspaceActiveChanged  = m_specialWorkspace->m_events.activeChanged.listen(VALIDATE);
}

void CSpecialWorkspaceGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_sessionActive)
        return;
    if (!ownsActiveSpecial()) {
        cancel();
        return;
    }

    g_pHyprRenderer->damageMonitor(m_monitor.lock());

    m_lastDelta += distance(e);

    const auto FADEPERCENT = m_animatingOut ? 1.F - std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F) : std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    m_monitor->m_specialFade->setValueAndWarp(std::lerp(m_monitorFadeFrom, m_monitorFadeTo, FADEPERCENT));
    m_monitor->m_specialDim->setValueAndWarp(std::lerp(m_monitorDimFrom, m_monitorDimTo, FADEPERCENT));
    m_monitor->m_specialBlur->setValueAndWarp(std::lerp(m_monitorBlurFrom, m_monitorBlurTo, FADEPERCENT));
    m_lastPreviewAlpha  = std::lerp(m_workspaceAlphaFrom, m_workspaceAlphaTo, FADEPERCENT);
    m_lastPreviewOffset = lerpVal(m_workspaceOffsetFrom, m_workspaceOffsetTo, FADEPERCENT);
    m_specialWorkspace->m_alpha->setValueAndWarp(m_lastPreviewAlpha);
    m_specialWorkspace->m_renderOffset->setValueAndWarp(m_lastPreviewOffset);
    m_hasPreview = true;
}

void CSpecialWorkspaceGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!m_sessionActive)
        return;

    const auto COMPLETION = std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    // Backend cancellation intentionally uses the same completion decision as a normal end.
    finish(COMPLETION < 0.3F ? !m_animatingOut : m_animatingOut);
}

void CSpecialWorkspaceGesture::finish(bool close) {
    const bool OWNS_SPECIAL   = m_sessionActive && ownsActiveSpecial();
    const auto WORKSPACE      = m_specialWorkspace;
    const auto MONITOR        = m_monitor.lock();
    const auto FADE_TO        = m_monitorFadeTo;
    const auto DIM_TO         = m_monitorDimTo;
    const auto BLUR_TO        = m_monitorBlurTo;
    const auto OFFSET_TO      = m_workspaceOffsetTo;
    const auto ALPHA_TO       = m_workspaceAlphaTo;
    const auto OWNER          = WORKSPACE ? WORKSPACE->m_monitor.lock() : nullptr;
    const bool MOVED_PREVIEW  = m_sessionActive && m_hasPreview && OWNER != MONITOR && isActiveSpecialOnMonitor(WORKSPACE, OWNER);
    const auto PREVIEW_ALPHA  = m_lastPreviewAlpha;
    const auto PREVIEW_OFFSET = m_lastPreviewOffset;

    // Detach before changing active state: cleanup must not recursively cancel itself.
    resetSession();

    if (!OWNS_SPECIAL) {
        if (!MOVED_PREVIEW)
            return;

        // Stealing an active special skips its IN animation. Resume only our untouched, warped preview;
        // the new owner has already established monitor fade/dim/blur and may have replaced either property.
        bool resumed = false;
        if (isActiveSpecialOnMonitor(WORKSPACE, OWNER) && !WORKSPACE->m_alpha->isBeingAnimated() && WORKSPACE->m_alpha->value() == PREVIEW_ALPHA &&
            WORKSPACE->m_alpha->goal() == PREVIEW_ALPHA) {
            *WORKSPACE->m_alpha = ALPHA_TO;
            resumed             = true;
        }
        if (isActiveSpecialOnMonitor(WORKSPACE, OWNER) && !WORKSPACE->m_renderOffset->isBeingAnimated() && WORKSPACE->m_renderOffset->value() == PREVIEW_OFFSET &&
            WORKSPACE->m_renderOffset->goal() == PREVIEW_OFFSET) {
            *WORKSPACE->m_renderOffset = OFFSET_TO;
            resumed                    = true;
        }
        if (resumed && isActiveSpecialOnMonitor(WORKSPACE, OWNER))
            g_pHyprRenderer->damageMonitor(OWNER);
        return;
    }

    if (close) {
        const auto CURR_WS_ALPHA  = WORKSPACE->m_alpha->value();
        const auto CURR_WS_OFFSET = WORKSPACE->m_renderOffset->value();
        const auto CURR_MON_FADE  = MONITOR->m_specialFade->value();
        const auto CURR_MON_DIM   = MONITOR->m_specialDim->value();
        const auto CURR_MON_BLUR  = MONITOR->m_specialBlur->value();

        MONITOR->setSpecialWorkspace(nullptr);

        // A synchronous listener may have installed new external state while we closed ours.
        if (!MONITOR->m_enabled || MONITOR->m_activeSpecialWorkspace || WORKSPACE->m_monitor != MONITOR || WORKSPACE->visible())
            return;

        const auto GOAL_WS_ALPHA  = WORKSPACE->m_alpha->goal();
        const auto GOAL_WS_OFFSET = WORKSPACE->m_renderOffset->goal();

        MONITOR->m_specialFade->setValueAndWarp(CURR_MON_FADE);
        MONITOR->m_specialDim->setValueAndWarp(CURR_MON_DIM);
        MONITOR->m_specialBlur->setValueAndWarp(CURR_MON_BLUR);
        WORKSPACE->m_alpha->setValueAndWarp(CURR_WS_ALPHA);
        WORKSPACE->m_renderOffset->setValueAndWarp(CURR_WS_OFFSET);

        *MONITOR->m_specialFade    = 0.F;
        *MONITOR->m_specialDim     = 0.F;
        *MONITOR->m_specialBlur    = 0.F;
        *WORKSPACE->m_alpha        = GOAL_WS_ALPHA;
        *WORKSPACE->m_renderOffset = GOAL_WS_OFFSET;
    } else {
        *MONITOR->m_specialFade    = FADE_TO;
        *MONITOR->m_specialDim     = DIM_TO;
        *MONITOR->m_specialBlur    = BLUR_TO;
        *WORKSPACE->m_renderOffset = OFFSET_TO;
        *WORKSPACE->m_alpha        = ALPHA_TO;
    }
}
