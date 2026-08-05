#include "WindowEffectsController.hpp"

#include <algorithm>
#include <cmath>

#include "Window.hpp"
#include "../../../managers/input/InputManager.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/transformer/MotionBlurTransformer.hpp"
#include "../../../render/transformer/TransformerList.hpp"
#include "../../../render/transformer/WobbleTransformer.hpp"
#include "../../../output/Monitor.hpp"

using namespace Desktop::View;

CWindowEffectsController::CWindowEffectsController(CWindow& window) : m_window(window), m_transformers(makeUnique<Render::CWindowTransformerList>()) {
    ;
}

CWindowEffectsController::~CWindowEffectsController() = default;

Render::CMotionBlurTransformer* CWindowEffectsController::motionBlurTransformer() {
    return m_transformers->get<Render::CMotionBlurTransformer>();
}

const Render::CMotionBlurTransformer* CWindowEffectsController::motionBlurTransformer() const {
    return m_transformers->get<Render::CMotionBlurTransformer>();
}

Render::CWobbleTransformer* CWindowEffectsController::wobbleTransformer() {
    return m_transformers->get<Render::CWobbleTransformer>();
}

std::optional<MotionBlur::SState> CWindowEffectsController::motionBlurState(bool allowStale) const {
    const auto MOTIONBLUR = motionBlurTransformer();
    if (!MOTIONBLUR)
        return std::nullopt;

    return MOTIONBLUR->state(allowStale);
}

void CWindowEffectsController::damageMotionBlur(bool allowStale) const {
    const auto STATE = motionBlurState(allowStale);
    if (!STATE)
        return;

    CBox damage = STATE->extents();
    damage.expand(4.F);
    g_pHyprRenderer->damageBox(damage);
}

void CWindowEffectsController::recordMotionBlur(const CBox& previous, const CBox& current) {
    if (previous == current || !Render::CMotionBlurTransformer::shouldEnable(m_window.m_self.lock())) {
        resetMotionBlur();
        return;
    }

    constexpr double MIN_EDGE_DELTA_PX = 2.0;

    const auto       PMONITOR = m_window.m_monitor.lock();
    const double     SCALE    = PMONITOR ? PMONITOR->m_scale : 1.0;
    const double     DELTA    = std::max({std::abs(previous.x - current.x), std::abs(previous.y - current.y), std::abs(previous.x + previous.w - current.x - current.w),
                                          std::abs(previous.y + previous.h - current.y - current.h)}) *
        SCALE;

    if (DELTA <= MIN_EDGE_DELTA_PX) {
        resetMotionBlur();
        return;
    }

    damageMotionBlur(true);

    auto MOTIONBLUR = motionBlurTransformer();
    if (!MOTIONBLUR)
        MOTIONBLUR = m_transformers->emplace<Render::CMotionBlurTransformer>(m_window.m_self);

    if (!MOTIONBLUR)
        return;

    MOTIONBLUR->record(previous, current);
    damageMotionBlur();
}

void CWindowEffectsController::resetMotionBlur() {
    damageMotionBlur(true);

    if (auto MOTIONBLUR = motionBlurTransformer())
        MOTIONBLUR->reset();

    m_transformers->removeInactive();
}

void CWindowEffectsController::resetWobble() {
    if (auto WOBBLE = wobbleTransformer())
        WOBBLE->resetWithDamage();

    m_transformers->removeInactive();
}

void CWindowEffectsController::reset() {
    resetMotionBlur();
    resetWobble();
}

void CWindowEffectsController::onPositionUpdate(const CBox& previous, const CBox& current, eWindowUpdateSource source) {
    recordMotionBlur(previous, current);

    if (previous == current)
        return;

    if (!Render::CWobbleTransformer::shouldEnable(m_window.m_self.lock())) {
        resetWobble();
        return;
    }

    auto WOBBLE = wobbleTransformer();
    if (!WOBBLE)
        WOBBLE = m_transformers->emplace<Render::CWobbleTransformer>(m_window.m_self);

    std::optional<Vector2D> grabPoint;
    if (source == WINDOW_UPDATE_MOUSE && current.w > 0.F && current.h > 0.F) {
        const auto MOUSE = g_pInputManager->getMouseCoordsInternal();
        grabPoint        = Vector2D{std::clamp((MOUSE.x - current.x) / current.w, 0.0, 1.0), std::clamp((MOUSE.y - current.y) / current.h, 0.0, 1.0)};
    }

    if (WOBBLE)
        WOBBLE->record(previous, current, grabPoint);
}

bool CWindowEffectsController::tickWobble() {
    const auto WOBBLE = wobbleTransformer();
    if (!WOBBLE)
        return false;

    const bool ACTIVE = WOBBLE->tick();
    m_transformers->removeInactive();
    return ACTIVE;
}

bool CWindowEffectsController::hasActiveTransformers() const {
    return !m_transformers->empty();
}

bool CWindowEffectsController::blocksDirectScanout() const {
    return m_transformers->blocksDirectScanout();
}

CBox CWindowEffectsController::transformedExtents(const CBox& currentBox) const {
    return m_transformers->transformedExtents(currentBox);
}

CBox CWindowEffectsController::transformBoxForDamage(const CBox& currentBox) const {
    return m_transformers->transformBoxForDamage(currentBox);
}

void CWindowEffectsController::preWindowRender(CSurfacePassElement::SRenderData* renderData) const {
    m_transformers->preWindowRender(renderData);
}

void CWindowEffectsController::amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* motionBlurData) const {
    m_transformers->amendTransformedRenderData(currentBox, motionBlurData);
}

const UP<Render::CWindowTransformerList>& CWindowEffectsController::transformers() const {
    return m_transformers;
}
