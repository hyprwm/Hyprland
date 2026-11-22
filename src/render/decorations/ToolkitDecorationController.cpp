#include "ToolkitDecorationController.hpp"
#include "CHyprToolkitDecoration.hpp"
#include "DecorationPositioner.hpp"
#include "ToolkitDecorationProvider.hpp"

#include <hyprtoolkit/core/EmbeddedBackend.hpp>
#include <hyprutils/string/VarList2.hpp>

#include <algorithm>

#include "../../desktop/rule/windowRule/WindowRuleApplicator.hpp"
#include "../../desktop/state/ViewHitTester.hpp"
#include "../../desktop/state/ViewState.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"
#include "../../event/EventBus.hpp"
#include "../../hyprtoolkit/Host.hpp"
#include "../../managers/SessionLockManager.hpp"
#include "../OpenGL.hpp"
#include "../Renderer.hpp"

using namespace Toolkit;

UP<CDecorationController>& Toolkit::decorations() {
    static UP<CDecorationController> p = makeUnique<CDecorationController>();
    return p;
}

static bool toolkitAvailable() {
    return Toolkit::host() && Toolkit::host()->backend();
}

static std::vector<std::string> wantedDecorations(PHLWINDOW window) {
    std::vector<std::string> names;

    for (const auto& name : Hyprutils::String::CVarList2(std::string{window->m_ruleApplicator->decorations().valueOrDefault()}, 0, 's', true)) {
        names.emplace_back(name);
    }

    return names;
}

CDecorationController::CDecorationController() {
    m_listeners.updateRules = Event::bus()->m_events.window.updateRules.listen([this](PHLWINDOW window) { syncWindow(window); });

    m_listeners.render = Event::bus()->m_events.render.stage.listen([this](const Event::SRenderStageEvent& event) {
        if (event.stage == RENDER_PRE)
            renderFor(event.monitor);
    });

    m_listeners.mouseMove = Event::bus()->m_events.input.mouse.move.listen([this](Vector2D coords, Event::SCallbackInfo&) { onMouseMove(coords); });
}

CDecorationController::~CDecorationController() = default;

bool CDecorationController::registerProvider(SP<IToolkitDecorationProvider> provider) {
    return m_providers.try_emplace(provider->name(), std::move(provider)).second;
}

void CDecorationController::unregisterProvider(const std::string& name) {
    const auto IT = m_providers.find(name);
    if (IT == m_providers.end())
        return;

    const auto PROVIDER = IT->second;
    m_providers.erase(IT);

    for (const auto& deco : liveDecorations()) {
        if (deco->provider() != PROVIDER)
            continue;

        deco->releaseClient();

        if (const auto WINDOW = deco->window())
            WINDOW->presentation().removeDecoration(deco.get());
    }
}

void CDecorationController::syncWindow(PHLWINDOW window) {
    if (!window || !toolkitAvailable())
        return;

    const auto WANTED = wantedDecorations(window);

    for (const auto& deco : liveDecorations()) {
        if (deco->window() != window || std::ranges::contains(WANTED, deco->provider()->name()))
            continue;

        window->presentation().removeDecoration(deco.get());
    }

    for (const auto& name : WANTED) {
        const auto IT = m_providers.find(name);
        if (IT == m_providers.end())
            continue;

        const auto PROVIDER = IT->second;
        const auto LIVE     = liveDecorations();
        if (std::ranges::any_of(LIVE, [&](const auto& deco) { return deco->window() == window && deco->provider() == PROVIDER; }))
            continue;

        // track and attach before building, the provider may end up syncing this window again
        const auto DECO = makeShared<CHyprToolkitDecoration>(window, PROVIDER);
        m_decorations.emplace_back(DECO);
        window->presentation().addDecoration(DECO);
        DECO->createClient();
    }
}

void CDecorationController::renderFor(PHLMONITOR monitor) {
    if (m_decorations.empty() || !toolkitAvailable())
        return;

    Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    for (const auto& deco : liveDecorations()) {
        const auto WINDOW = deco->window();
        if (!WINDOW || !g_pHyprRenderer->shouldRenderWindow(WINDOW, monitor))
            continue;

        deco->renderIfNeeded();
    }
}

void CDecorationController::onMouseMove(const Vector2D& coords) {
    if (m_decorations.empty())
        return;

    const auto DECO    = decorationAt(coords);
    const auto HOVERED = m_hovered.lock();

    if (HOVERED && HOVERED != DECO)
        HOVERED->pointerLeave();

    m_hovered = DECO;

    if (DECO)
        DECO->pointerMotion(coords);
}

SP<CHyprToolkitDecoration> CDecorationController::decorationAt(const Vector2D& coords) {
    if (g_pSessionLockManager && g_pSessionLockManager->isSessionLocked())
        return nullptr;

    const auto WINDOW = Desktop::viewState()->hitTest().windowAt(coords, Desktop::View::ALLOW_FLOATING | Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS);
    if (!WINDOW || WINDOW->hasPopupAt(coords))
        return nullptr;

    for (const auto& deco : liveDecorations()) {
        if (deco->window() != WINDOW || !(deco->getDecorationFlags() & DECORATION_ALLOWS_MOUSE_INPUT))
            continue;

        if (g_pDecorationPositioner->getWindowDecorationBox(deco.get()).containsPoint(coords))
            return deco;
    }

    return nullptr;
}

std::vector<SP<CHyprToolkitDecoration>> CDecorationController::liveDecorations() {
    std::erase_if(m_decorations, [](const auto& deco) { return deco.expired(); });

    std::vector<SP<CHyprToolkitDecoration>> live;
    live.reserve(m_decorations.size());

    for (const auto& deco : m_decorations) {
        live.emplace_back(deco.lock());
    }

    return live;
}
