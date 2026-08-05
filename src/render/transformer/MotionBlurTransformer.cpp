#include "MotionBlurTransformer.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/view/window/WindowEffectsController.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
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

    return *PMBENABLED && *PMBSAMPLES > 1 && !Fullscreen::controller()->isFullscreen(window);
}

SWindowTransformBuffer CMotionBlurTransformer::transform(const SWindowTransformBuffer& in, const SWindowTransformContext&) {
    return in;
}

int CMotionBlurTransformer::priority() const {
    return 100;
}

bool CMotionBlurTransformer::active() const {
    return state(true).has_value();
}

bool CMotionBlurTransformer::allocatesOutputBuffer() const {
    return false;
}

CBox CMotionBlurTransformer::sourceBoxForOutput(const CBox& outputBox, const CBox& inputBox) const {
    return outputBox.intersection(inputBox);
}

CBox CMotionBlurTransformer::transformBoxForDamage(const CBox& currentBox) const {
    const auto STATE = state(true);
    if (!STATE)
        return currentBox;

    const Vector2D relPos = currentBox.pos() - STATE->current.pos();
    const Vector2D scale  = STATE->previous.size() / STATE->current.size();

    CBox           previous = {STATE->previous.pos() + relPos * scale, currentBox.size() * scale};
    CBox           damaged  = MotionBlur::extents(previous, currentBox);
    damaged.expand(4.F);

    return damaged;
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

    const Vector2D RENDEROFFSET =
        ((PWINDOW->m_state & Desktop::View::WINDOW_STATE_PINNED) != Desktop::View::WINDOW_STATE_NONE || !PWINDOW->m_workspace ? Vector2D{} :
                                                                                                                                PWINDOW->m_workspace->m_renderOffset->value()) +
        PWINDOW->presentation().floatingOffset();
    return m_motionBlur.state(std::clamp(sc<int>(*PMBSAMPLES), 2, 64), RENDEROFFSET, allowStale);
}

void CMotionBlurTransformer::armExpiryTimer() {
    if (!m_expiryTimer) {
        m_expiryTimer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [window = m_window](SP<CEventLoopTimer>, void*) {
                if (const auto PWINDOW = window.lock())
                    PWINDOW->effects().resetMotionBlur();
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
