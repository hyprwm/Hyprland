#include "MotionBlurTransformer.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"
#include "../Renderer.hpp"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <chrono>

using namespace Render;

static bool boxIsFinite(const CBox& box) {
    return std::isfinite(box.x) && std::isfinite(box.y) && std::isfinite(box.w) && std::isfinite(box.h);
}

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

    return *PMBENABLED && *PMBSAMPLES > 1 && !window->isFullscreen() && !window->m_fadingOut;
}

SP<Render::IFramebuffer> CMotionBlurTransformer::transform(SP<Render::IFramebuffer> in) {
    return in;
}

void CMotionBlurTransformer::amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData) {

    if (!pMotionBlurData)
        return;

    assert(g_pHyprRenderer);

    const auto PMONITOR = g_pHyprRenderer->m_renderData.pMonitor;
    if (!PMONITOR)
        return;

    const auto STATE = state();
    if (!STATE)
        return;

    assert(boxIsFinite(STATE->previous));
    assert(boxIsFinite(STATE->current));
    assert(boxIsFinite(currentBox));

    assert(STATE->current.w > 0.F);
    assert(STATE->current.h > 0.F);

    SMotionBlurData motionBlur;
    motionBlur.enabled  = true;
    motionBlur.previous = STATE->previous.copy().translate(-PMONITOR->m_position);
    motionBlur.current  = STATE->current.copy().translate(-PMONITOR->m_position);
    motionBlur.samples  = STATE->samples;

    const Vector2D relPos = currentBox.pos() - motionBlur.current.pos();
    const Vector2D scale  = motionBlur.previous.size() / motionBlur.current.size();

    assert(std::isfinite(scale.x));
    assert(std::isfinite(scale.y));

    motionBlur.previous = {
        motionBlur.previous.pos() + relPos * scale,
        currentBox.size() * scale,
    };
    motionBlur.current = currentBox;

    assert(boxIsFinite(motionBlur.previous));
    assert(boxIsFinite(motionBlur.current));

    *pMotionBlurData = motionBlur;
}

void CMotionBlurTransformer::record(const CBox& previous, const CBox& current) {
    assert(boxIsFinite(previous));
    assert(boxIsFinite(current));

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

    assert(std::isfinite(RENDEROFFSET.x));
    assert(std::isfinite(RENDEROFFSET.y));

    const auto SAMPLES64 = std::clamp<Config::INTEGER>(*PMBSAMPLES, 2, 64);
    const auto SAMPLES   = sc<int>(SAMPLES64);

    assert(SAMPLES >= 2);
    assert(SAMPLES <= 64);

    return m_motionBlur.state(SAMPLES, RENDEROFFSET, allowStale);
}

void CMotionBlurTransformer::armExpiryTimer() {
    assert(g_pEventLoopManager);

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
    if (!m_expiryTimer)
        return;

    assert(g_pEventLoopManager);
    m_expiryTimer->updateTimeout(std::nullopt);
}
