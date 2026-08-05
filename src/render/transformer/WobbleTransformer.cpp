#include "WobbleTransformer.hpp"

#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/view/window/WindowEffectsController.hpp"
#include "../../desktop/state/WindowState.hpp"
#include "../../event/EventBus.hpp"
#include "../../output/Monitor.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../OpenGL.hpp"
#include "../Renderer.hpp"
#include "../pass/ClearPassElement.hpp"

#include <algorithm>
#include <cmath>

using namespace Render;
using namespace Hyprutils::Animation;

static CHyprSignalListener g_wobbleTickListener;
static constexpr double    WOBBLE_EXTENTS_PADDING = 4.0;

static void                tickWobbles() {
    bool anyActive = false;

    for (auto const& window : Desktop::windowState()->windows()) {
        if (!window)
            continue;

        anyActive = window->effects().tickWobble() || anyActive;
    }

    if (anyActive && Animation::mgr())
        Animation::mgr()->requestTick();
}

CWobbleTransformer::CWobbleTransformer(PHLWINDOWREF window) : m_window(window), m_mesh(8) {
    m_lastTick = std::chrono::steady_clock::now();
    ensureTickListener();
}

bool CWobbleTransformer::shouldEnable(PHLWINDOW window) {
    static auto PENABLED = CConfigValue<Config::INTEGER>("decoration:wobble:enabled");

    if (!window)
        return false;

    if (window->m_ruleApplicator->noWobble().valueOrDefault())
        return false;

    return *PENABLED && !Fullscreen::controller()->isFullscreen(window);
}

void CWobbleTransformer::ensureTickListener() {
    if (g_wobbleTickListener)
        return;

    g_wobbleTickListener = Event::bus()->m_events.tick.listen([] { tickWobbles(); });
}

SWindowTransformBuffer CWobbleTransformer::transform(const SWindowTransformBuffer& in, const SWindowTransformContext& context) {
    if (!m_active || context.standalone || context.renderingSnapshot || !context.monitor || !in.framebuffer || !in.framebuffer->getTexture())
        return in;

    const auto PWINDOW = m_window.lock();
    if (!shouldEnable(PWINDOW))
        return in;

    const double CANVASPADDING = 1.0 / context.monitor->m_scale;
    const auto   OUTPUTCANVAS  = pixelBoxForLogical(context.outputBox.copy().expand(CANVASPADDING), context.monitor->m_scale);
    if (OUTPUTCANVAS.empty())
        return in;

    const auto OUT = context.monitor->resources()->getUnusedWorkBuffer(OUTPUTCANVAS.size());
    if (!OUT)
        return {.framebuffer = in.framebuffer, .box = in.box, .success = false};

    const double SCALE          = context.monitor->m_scale;
    const CBox   SOURCEBOX      = context.currentBox.copy().scale(SCALE);
    const CBox   OUTPUTBOX      = transformedExtents(context.currentBox).scale(SCALE);
    const CBox   LOCALOUTPUTBOX = OUTPUTBOX.copy().translate(-OUTPUTCANVAS.pos());

    if (SOURCEBOX.empty() || OUTPUTBOX.empty() || OUTPUTCANVAS.empty())
        return in;

    const auto VERTICES = m_mesh.verticesForBox(SOURCEBOX, OUTPUTBOX, in.framebuffer->getTexture()->m_size, SCALE, HYPRUTILS_TRANSFORM_NORMAL, in.box.pos());
    if (VERTICES.empty())
        return {.framebuffer = in.framebuffer, .box = in.box, .success = false};

    auto&         renderData    = g_pHyprRenderer->m_renderData;
    const CRegion oldDamage     = renderData.damage.copy();
    const auto    oldProjection = renderData.projectionType;
    const auto    oldFBSize     = renderData.fbSize;

    {
        auto guard        = g_pHyprRenderer->bindTempFB(OUT);
        renderData.damage = CRegion{0, 0, sc<int>(OUTPUTCANVAS.w), sc<int>(OUTPUTCANVAS.h)};
        renderData.fbSize = OUTPUTCANVAS.size();
        g_pHyprRenderer->setProjectionType(RPT_EXPORT);

        g_pHyprRenderer->draw(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)});
        GL::g_pHyprOpenGL->renderTextureMesh(in.framebuffer->getTexture(), LOCALOUTPUTBOX, VERTICES,
                                             GL::CHyprOpenGLImpl::STextureRenderData{.damage = &renderData.damage, .a = 1.F, .allowCustomUV = true});
    }

    renderData.damage = oldDamage;
    renderData.fbSize = oldFBSize;
    g_pHyprRenderer->setProjectionType(oldProjection);
    return {.framebuffer = OUT, .box = OUTPUTCANVAS};
}

int CWobbleTransformer::priority() const {
    return 10;
}

bool CWobbleTransformer::active() const {
    const auto PWINDOW = m_window.lock();
    return m_active && shouldEnable(PWINDOW);
}

bool CWobbleTransformer::blocksDirectScanout() const {
    return active();
}

CBox CWobbleTransformer::transformedExtents(const CBox& currentBox) const {
    if (!m_active)
        return currentBox;

    return m_mesh.transformedExtents(currentBox).expand(WOBBLE_EXTENTS_PADDING);
}

CBox CWobbleTransformer::sourceBoxForOutput(const CBox& outputBox, const CBox& inputBox) const {
    if (!m_active)
        return outputBox.intersection(inputBox);

    return m_mesh.sourceBoxForOutput(inputBox, outputBox).expand(WOBBLE_EXTENTS_PADDING).intersection(inputBox);
}

void CWobbleTransformer::record(const CBox& previous, const CBox& current, std::optional<Vector2D> grabPoint) {
    const auto PWINDOW = m_window.lock();
    if (!shouldEnable(PWINDOW)) {
        resetWithDamage();
        return;
    }

    if (previous == current)
        return;

    const Vector2D DELTA        = current.pos() - previous.pos();
    const Vector2D SIZEDELTA    = current.size() - previous.size();
    const double   MAXDELTA     = std::max({std::abs(DELTA.x), std::abs(DELTA.y), std::abs(SIZEDELTA.x), std::abs(SIZEDELTA.y)});
    const double   MAXDIMENSION = std::max({previous.w, previous.h, current.w, current.h, 1.0});
    if (MAXDELTA > MAXDIMENSION * 1.5) {
        resetWithDamage();
        return;
    }

    static auto PMESH      = CConfigValue<Config::INTEGER>("decoration:wobble:mesh");
    static auto PINTENSITY = CConfigValue<Config::FLOAT>("decoration:wobble:intensity");

    damageCurrent();

    m_mesh.setSize(std::clamp(sc<size_t>(*PMESH), static_cast<size_t>(2), static_cast<size_t>(32)));
    m_mesh.onPositionUpdate(previous, current, *PINTENSITY, grabPoint);
    if (!m_active)
        m_lastTick = std::chrono::steady_clock::now();
    m_active = true;

    damageCurrent();
    scheduleFrame();

    ensureTickListener();
    if (Animation::mgr())
        Animation::mgr()->requestTick();
}

void CWobbleTransformer::reset() {
    m_mesh.reset();
    m_active = false;
}

void CWobbleTransformer::resetWithDamage() {
    damageCurrent();
    reset();
    damageCurrent();
    scheduleFrame();
}

bool CWobbleTransformer::tick() {
    const auto PWINDOW = m_window.lock();
    if (!m_active || !shouldEnable(PWINDOW)) {
        resetWithDamage();
        return false;
    }

    const auto NOW = std::chrono::steady_clock::now();
    const auto DT  = NOW - m_lastTick;
    m_lastTick     = NOW;

    damageCurrent();
    m_mesh.advance(spring(), DT);
    damageCurrent();

    static auto PVALUEEPS = CConfigValue<Config::FLOAT>("decoration:wobble:value_epsilon");
    static auto PVELEPS   = CConfigValue<Config::FLOAT>("decoration:wobble:velocity_epsilon");

    if (m_mesh.stable(*PVALUEEPS, *PVELEPS)) {
        resetWithDamage();
        return false;
    }

    scheduleFrame();

    return true;
}

void CWobbleTransformer::damageCurrent() const {
    const auto PWINDOW = m_window.lock();
    if (PWINDOW)
        g_pHyprRenderer->damageWindow(PWINDOW);
}

void CWobbleTransformer::scheduleFrame() const {
    const auto PWINDOW = m_window.lock();
    if (!PWINDOW)
        return;

    if (const auto PMONITOR = PWINDOW->m_monitor.lock())
        PMONITOR->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_ANIMATION);
}

SSpringCurve CWobbleTransformer::spring() const {
    static auto PSTIFFNESS = CConfigValue<Config::FLOAT>("decoration:wobble:stiffness");
    static auto PDAMPING   = CConfigValue<Config::FLOAT>("decoration:wobble:damping");
    static auto PMASS      = CConfigValue<Config::FLOAT>("decoration:wobble:mass");
    static auto PVALUEEPS  = CConfigValue<Config::FLOAT>("decoration:wobble:value_epsilon");
    static auto PVELEPS    = CConfigValue<Config::FLOAT>("decoration:wobble:velocity_epsilon");

    return SSpringCurve{
        .stiffness       = *PSTIFFNESS,
        .damping         = *PDAMPING,
        .mass            = *PMASS,
        .valueEpsilon    = *PVALUEEPS,
        .velocityEpsilon = *PVELEPS,
    };
}
