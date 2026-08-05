#include "WindowPresentation.hpp"

#include "Window.hpp"
#include "../../../animation/AnimationManager.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../config/shared/animation/AnimationTree.hpp"
#include "../../../layout/target/Target.hpp"
#include "../../../managers/fullscreen/FullscreenController.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../protocols/core/Compositor.hpp"
#include "../../../render/decorations/CHyprBorderDecoration.hpp"
#include "../../../render/decorations/CHyprDropShadowDecoration.hpp"
#include "../../../render/decorations/CHyprInnerGlowDecoration.hpp"
#include "../../../render/decorations/DecorationPositioner.hpp"
#include "../../state/FocusState.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>

using namespace Desktop;
using namespace Desktop::View;

class CDecorationSnapshot {
  public:
    CDecorationSnapshot(std::span<const SP<IHyprWindowDecoration>> decorations) {
        if (decorations.size() <= m_inline.size()) {
            std::ranges::copy(decorations, m_inline.begin());
            m_decorations = std::span<const SP<IHyprWindowDecoration>>{m_inline}.first(decorations.size());
            return;
        }

        m_fallback.assign(decorations.begin(), decorations.end());
        m_decorations = m_fallback;
    }

    CDecorationSnapshot(const CDecorationSnapshot&)                                  = delete;
    CDecorationSnapshot(CDecorationSnapshot&&)                                       = delete;
    CDecorationSnapshot&                       operator=(const CDecorationSnapshot&) = delete;
    CDecorationSnapshot&                       operator=(CDecorationSnapshot&&)      = delete;

    std::span<const SP<IHyprWindowDecoration>> decorations() const {
        return m_decorations;
    }

  private:
    std::array<SP<IHyprWindowDecoration>, 4>   m_inline;
    std::vector<SP<IHyprWindowDecoration>>     m_fallback;
    std::span<const SP<IHyprWindowDecoration>> m_decorations;
};

CWindowPresentation::CWindowPresentation(CWindow& window) : m_window(window), m_alpha(WINDOW_ALPHA_LAST), m_animationController(&window) {
    ;
}

CWindowPresentation::~CWindowPresentation() = default;

void CWindowPresentation::initialize() {
    const auto WINDOW  = m_window.m_self.lock();
    m_shadowDecoration = makeShared<CHyprDropShadowDecoration>(WINDOW);
    m_borderDecoration = makeShared<CHyprBorderDecoration>(WINDOW);
    m_glowDecoration   = makeShared<CHyprInnerGlowDecoration>(WINDOW);

    addDecorationInternal(m_shadowDecoration);
    addDecorationInternal(m_borderDecoration);
    addDecorationInternal(m_glowDecoration);

    m_borderDecoration->initializeAnimations();
    Animation::mgr()->createAnimation(1.F, alpha(WINDOW_ALPHA_FADE), Config::animationTree()->getAnimationPropertyConfig("fadeIn"), WINDOW, AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(1.F, alpha(WINDOW_ALPHA_ACTIVE), Config::animationTree()->getAnimationPropertyConfig("fadeSwitch"), WINDOW, AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(1.F, alpha(WINDOW_ALPHA_FULLSCREEN), Config::animationTree()->getAnimationPropertyConfig("fadeIn"), WINDOW, AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(1.F, alpha(WINDOW_ALPHA_LAYOUT), Config::animationTree()->getAnimationPropertyConfig("fadeSwitch"), WINDOW, AVARDAMAGE_ENTIRE);
    m_shadowDecoration->initializeAnimations();
    m_glowDecoration->initializeAnimations();
    Animation::mgr()->createAnimation(0.F, m_dimPercent, Config::animationTree()->getAnimationPropertyConfig("fadeDim"), WINDOW, AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(1.F, alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE), Config::animationTree()->getAnimationPropertyConfig("fadeOut"), WINDOW, AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(1.F, alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE), Config::animationTree()->getAnimationPropertyConfig("fadeIn"), WINDOW, AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(0.F, m_notRespondingTint, Config::animationTree()->getAnimationPropertyConfig("fade"), WINDOW, AVARDAMAGE_ENTIRE);
}

std::span<const SP<IHyprWindowDecoration>> CWindowPresentation::decorations() const {
    return m_windowDecorations;
}

bool CWindowPresentation::containsDecoration(const IHyprWindowDecoration* decoration) const {
    return std::ranges::find_if(m_windowDecorations, [decoration](const auto& candidate) { return candidate.get() == decoration; }) != m_windowDecorations.end();
}

SP<IHyprWindowDecoration> CWindowPresentation::decoration(eDecorationType type) const {
    const auto IT = std::ranges::find_if(m_windowDecorations, [type](const auto& candidate) { return candidate->getDecorationType() == type; });
    return IT == m_windowDecorations.end() ? nullptr : *IT;
}

void CWindowPresentation::addDecoration(SP<IHyprWindowDecoration> decoration) {
    if (!decoration)
        return;

    addDecorationInternal(decoration);
    decoration->initializeAnimations();
    decoration->updateState();
    if (m_window.mapped())
        decoration->onWindowMap();

    g_pDecorationPositioner->forceRecalcFor(m_window.m_self.lock());
    updateDecorations();

    if (m_window.layoutTarget())
        m_window.layoutTarget()->recalc();
}

void CWindowPresentation::addDecorationInternal(const SP<IHyprWindowDecoration>& decoration) {
    decoration->setSelf(decoration);
    m_windowDecorations.emplace_back(decoration);
}

void CWindowPresentation::removeDecoration(IHyprWindowDecoration* decoration) {
    if (!decoration || decoration == m_shadowDecoration.get() || decoration == m_borderDecoration.get() || decoration == m_glowDecoration.get())
        return;

    m_decosToRemove.push_back(decoration);
    g_pDecorationPositioner->forceRecalcFor(m_window.m_self.lock());
    updateDecorations();

    if (m_window.layoutTarget())
        m_window.layoutTarget()->recalc();
}

void CWindowPresentation::updateDecorations() {
    for (const auto& pending : m_decosToRemove) {
        const auto IT = std::ranges::find_if(m_windowDecorations, [pending](const auto& decoration) { return decoration.get() == pending; });
        if (IT == m_windowDecorations.end())
            continue;

        g_pDecorationPositioner->uncacheDecoration(IT->get());
        m_windowDecorations.erase(IT);
    }

    m_decosToRemove.clear();

    if (!m_window.mapped() || m_window.isHidden())
        return;

    g_pDecorationPositioner->onWindowUpdate(m_window.m_self.lock());

    const CDecorationSnapshot SNAPSHOT{m_windowDecorations};
    for (const auto& decoration : SNAPSHOT.decorations()) {
        if (!containsDecoration(decoration.get()))
            continue;
        decoration->updateWindow(m_window.m_self.lock());
    }
}

void CWindowPresentation::uncacheDecorations() {
    for (const auto& decoration : m_windowDecorations)
        g_pDecorationPositioner->uncacheDecoration(decoration.get());
}

bool CWindowPresentation::checkInputOnDecorations(eInputType type, const Vector2D& coordinates, std::any data) {
    if (type != INPUT_TYPE_DRAG_END && m_window.hasPopupAt(coordinates))
        return false;

    const CDecorationSnapshot SNAPSHOT{m_windowDecorations};
    for (const auto& decoration : SNAPSHOT.decorations()) {
        if (!containsDecoration(decoration.get()))
            continue;
        if (!(decoration->getDecorationFlags() & DECORATION_ALLOWS_MOUSE_INPUT))
            continue;
        if (!g_pDecorationPositioner->getWindowDecorationBox(decoration.get()).containsPoint(coordinates))
            continue;
        if (decoration->onInputOnDeco(type, coordinates, data))
            return true;
    }

    return false;
}

Types::CMultiAVarContainer<float, uint8_t>& CWindowPresentation::alpha() {
    return m_alpha;
}

const Types::CMultiAVarContainer<float, uint8_t>& CWindowPresentation::alpha() const {
    return m_alpha;
}

PHLANIMVAR<float>& CWindowPresentation::alpha(eWindowAlpha type) {
    return m_alpha.get(type);
}

const PHLANIMVAR<float>& CWindowPresentation::alpha(eWindowAlpha type) const {
    return m_alpha.get(type);
}

float CWindowPresentation::alphaValue(eWindowAlpha type) const {
    return alpha(type)->value();
}

float CWindowPresentation::alphaGoal(eWindowAlpha type) const {
    return alpha(type)->goal();
}

float CWindowPresentation::alphaTotal() const {
    return m_alpha.getTotal();
}

float CWindowPresentation::alphaTotalGoal() const {
    return m_alpha.getTotalGoal();
}

int CWindowPresentation::borderSize() const {
    return m_borderDecoration->borderSize();
}

void CWindowPresentation::invalidateBorderSize() {
    m_borderDecoration->invalidateBorderSize();
}

bool CWindowPresentation::opaque() {
    if (alphaValue(WINDOW_ALPHA_FADE) != 1.f || alphaValue(WINDOW_ALPHA_FULLSCREEN) != 1.f || alphaValue(WINDOW_ALPHA_ACTIVE) != 1.f)
        return false;

    const auto PWORKSPACE = m_window.m_workspace;

    if (m_window.wlSurface()->small() && !m_window.wlSurface()->m_fillIgnoreSmall)
        return false;

    if (PWORKSPACE && PWORKSPACE->m_alpha->value() != 1.f)
        return false;

    auto solitaryResource = m_window.getSolitaryResource();
    if (!solitaryResource || !solitaryResource->m_current.texture)
        return false;

    if (m_window.backend().isX11())
        return solitaryResource->m_current.texture->m_opaque;

    // TODO: this is wrong
    const auto EXTENTS = solitaryResource->m_current.opaque.getExtents();
    if (EXTENTS.w >= solitaryResource->m_current.bufferSize.x && EXTENTS.h >= solitaryResource->m_current.bufferSize.y)
        return true;

    return solitaryResource->m_current.texture->m_opaque;
}

float CWindowPresentation::rounding() {
    static auto PROUNDING      = CConfigValue<Config::INTEGER>("decoration:rounding");
    static auto PROUNDINGPOWER = CConfigValue<Config::FLOAT>("decoration:rounding_power");

    float       roundingPower = m_window.m_ruleApplicator->roundingPower().valueOr(*PROUNDINGPOWER);
    float       rounding      = m_window.m_ruleApplicator->rounding().valueOr(*PROUNDING) * (roundingPower / 2.0); /* Make perceived roundness consistent. */

    return rounding;
}

float CWindowPresentation::roundingPower() {
    static auto PROUNDINGPOWER = CConfigValue<Config::FLOAT>("decoration:rounding_power");

    return m_window.m_ruleApplicator->roundingPower().valueOr(std::clamp(*PROUNDINGPOWER, 1.F, 10.F));
}

// Check if the point is hidden under a rounded corner. The point is assumed to
// be within the real window box; behavior outside it is undefined.
bool CWindowPresentation::isInCurvedCorner(double x, double y) {
    const int ROUNDING      = rounding();
    const int ROUNDINGPOWER = roundingPower();
    if (borderSize() >= ROUNDING)
        return false;

    // (x0, y0), (x0, y1), ... are the center point of rounding at each corner
    const auto POSITION = m_window.position(IGeometric::GEOMETRIC_CURRENT);
    const auto SIZE     = m_window.size(IGeometric::GEOMETRIC_CURRENT);
    double     x0       = POSITION.x + ROUNDING;
    double     y0       = POSITION.y + ROUNDING;
    double     x1       = POSITION.x + SIZE.x - ROUNDING;
    double     y1       = POSITION.y + SIZE.y - ROUNDING;

    if (x < x0 && y < y0)
        return std::pow(x0 - x, ROUNDINGPOWER) + std::pow(y0 - y, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);
    if (x > x1 && y < y0)
        return std::pow(x - x1, ROUNDINGPOWER) + std::pow(y0 - y, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);
    if (x < x0 && y > y1)
        return std::pow(x0 - x, ROUNDINGPOWER) + std::pow(y - y1, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);
    if (x > x1 && y > y1)
        return std::pow(x - x1, ROUNDINGPOWER) + std::pow(y - y1, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);

    return false;
}

bool CWindowPresentation::visibleOnMonitor(PHLMONITOR monitor) {
    CBox windowBox = {m_window.position(IGeometric::GEOMETRIC_CURRENT), m_window.size(IGeometric::GEOMETRIC_CURRENT)};

    if (m_window.isFloating())
        windowBox = m_window.getFullWindowBoundingBox();

    return !windowBox.intersection({monitor->m_position, monitor->m_size}).empty();
}

float CWindowPresentation::dimPercent() const {
    return m_dimPercent->value();
}

void CWindowPresentation::setDimPercent(float percent) {
    *m_dimPercent = percent;
}

void CWindowPresentation::warpDimPercent(float percent) {
    m_dimPercent->setValueAndWarp(percent);
}

float CWindowPresentation::notRespondingTint() const {
    return m_notRespondingTint->value();
}

void CWindowPresentation::setNotResponding(bool notResponding) {
    const float GOAL = notResponding ? 0.2F : 0.F;
    if (m_notRespondingTint->goal() != GOAL)
        *m_notRespondingTint = GOAL;
}

const Vector2D& CWindowPresentation::floatingOffset() const {
    return m_floatingOffset;
}

void CWindowPresentation::setFloatingOffset(const Vector2D& offset) {
    m_floatingOffset = offset;
}

void CWindowPresentation::clearFloatingOffset() {
    m_floatingOffset = {};
}

bool CWindowPresentation::movingFromMonitor() const {
    return m_monitorMovedFrom != -1;
}

void CWindowPresentation::setMonitorMovedFrom(int monitor) {
    m_monitorMovedFrom = monitor;
}

void CWindowPresentation::resetMonitorMovedFrom() {
    m_monitorMovedFrom = -1;
}

bool CWindowPresentation::animatingIn() const {
    return m_animatingIn;
}

void CWindowPresentation::setAnimatingIn(bool animating) {
    m_animatingIn = animating;
}

void CWindowPresentation::prepareMap() {
    alpha(WINDOW_ALPHA_ACTIVE)->resetAllCallbacks();
    alpha(WINDOW_ALPHA_FADE)->resetAllCallbacks();
    alpha(WINDOW_ALPHA_FULLSCREEN)->resetAllCallbacks();
    alpha(WINDOW_ALPHA_LAYOUT)->resetAllCallbacks();
    m_dimPercent->resetAllCallbacks();
    alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE)->resetAllCallbacks();
    alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->resetAllCallbacks();

    alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(1.F);
    alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE)->setValueAndWarp(1.F);
}

void CWindowPresentation::dispatchMap() {
    const CDecorationSnapshot SNAPSHOT{m_windowDecorations};
    for (const auto& decoration : SNAPSHOT.decorations()) {
        if (!containsDecoration(decoration.get()))
            continue;
        decoration->onWindowMap();
    }
}

void CWindowPresentation::setAnimationsToMove() {
    m_window.positionAnimation()->setConfig(Config::animationTree()->getAnimationPropertyConfig("windowsMove"));
    m_animatingIn = false;
}

void CWindowPresentation::onWorkspaceAnimUpdate() {
    if (!m_window.isFloating() || (m_window.m_state & WINDOW_STATE_PINNED) != WINDOW_STATE_NONE || Fullscreen::controller()->isFullscreen(m_window.m_self.lock())) {
        clearFloatingOffset();
        return;
    }

    const auto WORKSPACE = m_window.m_workspace;
    if (!WORKSPACE)
        return;

    const auto MONITOR = m_window.m_monitor.lock();
    if (!MONITOR)
        return;

    Vector2D   offset;
    const auto WINDOW_BOX = m_window.getFullWindowBoundingBox();
    if (WORKSPACE->m_renderOffset->value().x != 0) {
        const auto PROGRESS = WORKSPACE->m_renderOffset->value().x / MONITOR->m_size.x;

        if (WINDOW_BOX.x < MONITOR->m_position.x)
            offset.x += (MONITOR->m_position.x - WINDOW_BOX.x) * PROGRESS;
        if (WINDOW_BOX.x + WINDOW_BOX.width > MONITOR->m_position.x + MONITOR->m_size.x)
            offset.x += (WINDOW_BOX.x + WINDOW_BOX.width - MONITOR->m_position.x - MONITOR->m_size.x) * PROGRESS;
    } else if (WORKSPACE->m_renderOffset->value().y != 0) {
        const auto PROGRESS = WORKSPACE->m_renderOffset->value().y / MONITOR->m_size.y;

        if (WINDOW_BOX.y < MONITOR->m_position.y)
            offset.y += (MONITOR->m_position.y - WINDOW_BOX.y) * PROGRESS;
        if (WINDOW_BOX.y + WINDOW_BOX.height > MONITOR->m_position.y + MONITOR->m_size.y)
            offset.y += (WINDOW_BOX.y + WINDOW_BOX.height - MONITOR->m_position.y - MONITOR->m_size.y) * PROGRESS;
    }

    m_floatingOffset = offset;
}

void CWindowPresentation::onFocusAnimUpdate() {
    const CDecorationSnapshot SNAPSHOT{m_windowDecorations};
    for (const auto& decoration : SNAPSHOT.decorations()) {
        if (!containsDecoration(decoration.get()))
            continue;
        decoration->onWindowFocus();
    }
}

void CWindowPresentation::refreshValues() {
    static auto PINACTIVEALPHA   = CConfigValue<Config::FLOAT>("decoration:inactive_opacity");
    static auto PACTIVEALPHA     = CConfigValue<Config::FLOAT>("decoration:active_opacity");
    static auto PFULLSCREENALPHA = CConfigValue<Config::FLOAT>("decoration:fullscreen_opacity");
    static auto PDIMSTRENGTH     = CConfigValue<Config::FLOAT>("decoration:dim_strength");
    static auto PDIMENABLED      = CConfigValue<Config::INTEGER>("decoration:dim_inactive");
    static auto PDIMMODAL        = CConfigValue<Config::INTEGER>("decoration:dim_modal");

    const bool  IS_SHADOWED_BY_MODAL = m_window.backend().traits().hasModalChild;

    if (Fullscreen::controller()->getFullscreenModes(m_window.m_self.lock()).internal == Fullscreen::FSMODE_FULLSCREEN)
        *alpha(WINDOW_ALPHA_ACTIVE) = m_window.m_ruleApplicator->alphaFullscreen().valueOrDefault().applyAlpha(*PFULLSCREENALPHA);
    else if (m_window.m_self == Desktop::focusState()->window())
        *alpha(WINDOW_ALPHA_ACTIVE) = m_window.m_ruleApplicator->alpha().valueOrDefault().applyAlpha(*PACTIVEALPHA);
    else
        *alpha(WINDOW_ALPHA_ACTIVE) = m_window.m_ruleApplicator->alphaInactive().valueOrDefault().applyAlpha(*PINACTIVEALPHA);

    float goalDim = 1.F;
    if (m_window.m_self == Desktop::focusState()->window() || m_window.m_ruleApplicator->noDim().valueOrDefault() || !*PDIMENABLED)
        goalDim = 0.F;
    else
        goalDim = *PDIMSTRENGTH;

    if (IS_SHADOWED_BY_MODAL && *PDIMMODAL)
        goalDim += (1.F - goalDim) / 2.F;

    setDimPercent(goalDim);

    const CDecorationSnapshot SNAPSHOT{m_windowDecorations};
    for (const auto& decoration : SNAPSHOT.decorations()) {
        if (!containsDecoration(decoration.get()))
            continue;
        decoration->updateState();
    }

    updateDecorations();
}

Animation::SViewAnimationContext CWindowPresentation::animateOut() const {
    return m_animationController.animateOut();
}

void CWindowPresentation::applyAnimateIn() const {
    m_animationController.apply(m_animationController.animateIn());
}
