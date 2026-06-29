#include "MotionBlurTransformer.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"
#include "../Renderer.hpp"

#include <algorithm>
#include <chrono>

using namespace Render;

CMotionBlurTransformer::CMotionBlurTransformer(PHLWINDOWREF window) : m_window(window) {
    ;
}

CMotionBlurTransformer::~CMotionBlurTransformer() {
    disarmExpiryTimer();
}

bool CMotionBlurTransformer::shouldEnable(PHLWINDOW window) {
    static auto PMBENABLED = CConfigValue<Config::INTEGER>("decoration:motion_blur:enabled");
    static auto PMBSAMPLES = CConfigValue<Config::INTEGER>("decoration:motion_blur:samples");

    if (!window)
        return false;

    return *PMBENABLED && *PMBSAMPLES > 1 && !window->isFullscreen();
}

SP<Render::IFramebuffer> CMotionBlurTransformer::transform(SP<Render::IFramebuffer> in) {
    return in;
}

void CMotionBlurTransformer::amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData) {
    if (!pMotionBlurData)
        return;

    const auto PMONITOR = g_pHyprRenderer->m_renderData.pMonitor;
    if (!PMONITOR)
        return;

    const auto STATE = state();
    if (!STATE)
        return;

    SMotionBlurData motionBlur;
    motionBlur.enabled  = true;
    motionBlur.previous = STATE->previous.copy().translate(-PMONITOR->m_position);
    motionBlur.current  = STATE->current.copy().translate(-PMONITOR->m_position);
    motionBlur.samples  = STATE->samples;

    const Vector2D relPos = currentBox.pos() - motionBlur.current.pos();
    const Vector2D scale  = motionBlur.previous.size() / motionBlur.current.size();

    motionBlur.previous = {motionBlur.previous.pos() + relPos * scale, currentBox.size() * scale};
    motionBlur.current  = currentBox;

    *pMotionBlurData = motionBlur;
}

void CMotionBlurTransformer::record(const CBox& previous, const CBox& current) {
    m_motionBlur.record(previous, current);
    armExpiryTimer();
}

void CMotionBlurTransformer::reset() {
    m_motionBlur.reset();
    disarmExpiryTimer();
}

std::optional<MotionBlur::SState> CMotionBlurTransformer::state(bool allowStale) const {
    const auto PWINDOW = m_window.lock();
    if (!shouldEnable(PWINDOW))
        return std::nullopt;

    static auto    PMBSAMPLES = CConfigValue<Config::INTEGER>("decoration:motion_blur:samples");

    const Vector2D RENDEROFFSET = (PWINDOW->m_pinned || !PWINDOW->m_workspace ? Vector2D{} : PWINDOW->m_workspace->m_renderOffset->value()) + PWINDOW->m_floatingOffset;
    return m_motionBlur.state(std::clamp(sc<int>(*PMBSAMPLES), 2, 64), RENDEROFFSET, allowStale);
}

void CMotionBlurTransformer::armExpiryTimer() {
    if (!m_expiryTimer) {
        m_expiryTimer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [window = m_window](SP<CEventLoopTimer>, void*) {
                if (const auto PWINDOW = window.lock())
                    PWINDOW->resetMotionBlur();
            },
            nullptr);
        g_pEventLoopManager->addTimer(m_expiryTimer);
    }

    m_expiryTimer->updateTimeout(std::chrono::milliseconds(110));
}

void CMotionBlurTransformer::disarmExpiryTimer() {
    if (m_expiryTimer)
        m_expiryTimer->updateTimeout(std::nullopt);
}
