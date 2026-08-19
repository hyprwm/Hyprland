#include "OverviewScene.hpp"

#include "WorkspaceTapeController.hpp"
#include "WorkspaceSearch.hpp"
#include "WorkspaceSearchController.hpp"
#include "../Overview.hpp"
#include "../../../desktop/Workspace.hpp"
#include "../../../desktop/DesktopTypes.hpp"
#include "../../../desktop/state/WindowState.hpp"
#include "../../../desktop/view/window/Window.hpp"
#include "../../../desktop/view/window/WindowMetadata.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../output/MonitorResources.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/pass/ClearPassElement.hpp"
#include "../../../render/pass/RectPassElement.hpp"

#include <algorithm>

using namespace Overview::Hyprland;

static constexpr float  OVERVIEW_GRAY                = 0.16F;
static constexpr float  BACKGROUND_DIM               = 0.18F;
static constexpr size_t BACKGROUND_BLUR_WORK_BUFFERS = 2;

COverviewScene::COverviewScene(COverview& parent) :
    m_parent(parent), m_workspaceTape(makeUnique<CWorkspaceTapeController>()), m_workspaceSearch(makeUnique<CWorkspaceSearchController>()) {
    ;
}

COverviewScene::~COverviewScene() = default;

void COverviewScene::draw(Render::CRenderingContext& context, Time::steady_tp tp) {
    const auto MONITOR = m_parent.m_monitor.lock();
    if (!MONITOR || !m_parent.m_progress || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->addPassElement(context, makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(OVERVIEW_GRAY, OVERVIEW_GRAY, OVERVIEW_GRAY, 1.F)}));

    static auto PXPMODE = CConfigValue<Config::INTEGER>("render:xp_mode");
    if (!*PXPMODE)
        g_pHyprRenderer->renderMonitorBackground(context, MONITOR, tp);

    CRectPassElement::SRectData backgroundEffect;
    backgroundEffect.box   = {{}, MONITOR->m_transformedSize};
    backgroundEffect.color = CHyprColor(0.F, 0.F, 0.F, BACKGROUND_DIM);
    backgroundEffect.blur  = !*PXPMODE;
    g_pHyprRenderer->addPassElement(context, makeUnique<CRectPassElement>(backgroundEffect));

    m_workspaceTape->draw(context, tp, std::clamp(m_parent.m_progress->value(), 0.F, 1.F), *PXPMODE ? 0 : BACKGROUND_BLUR_WORK_BUFFERS);
    m_workspaceSearch->draw(context, std::clamp(m_parent.m_progress->value(), 0.F, 1.F));
}

static uint8_t foldASCII(uint8_t character) {
    if (character >= 'A' && character <= 'Z')
        return character + ('a' - 'A');
    return character;
}

bool Overview::Hyprland::matchesName(std::string_view name, std::string_view query) {
    if (query.empty())
        return true;

    return std::ranges::search(name, query, [](char lhs, char rhs) { return foldASCII(sc<unsigned char>(lhs)) == foldASCII(sc<unsigned char>(rhs)); }).begin() != name.end();
}

static bool workspaceFilter(PHLWORKSPACE ws, const std::string& query) {
    if (!ws)
        return false;

    if (matchesName(ws->m_name, query))
        return true;

    // check windows, we can match by title or class.
    for (const auto& w : Desktop::windowState()->windows()) {
        if (w->m_workspace != ws)
            continue;

        if (matchesName(w->metadata().appID(), query) || matchesName(w->metadata().title(), query))
            return true;
    }

    return false;
}

void COverviewScene::start(PHLMONITOR monitor, WP<Monitor::CMonitorResources> resources) {
    m_layout = monitor ? OverviewLayout::calculate(monitor->m_size, monitor->m_scale) : OverviewLayout::SLayout{};
    m_workspaceTape->setFilter([](PHLWORKSPACE) { return true; });
    m_workspaceTape->start(monitor, resources, m_layout);
    m_workspaceSearch->start(monitor, [this](const std::string& query) { m_workspaceTape->setFilter([query](PHLWORKSPACE w) { return ::workspaceFilter(w, query); }); });
}

bool COverviewScene::navigateLeft() {
    return m_workspaceTape->navigateLeft();
}

bool COverviewScene::navigateRight() {
    return m_workspaceTape->navigateRight();
}

bool COverviewScene::selectWorkspace(PHLWORKSPACE workspace) {
    return m_workspaceTape->selectWorkspace(workspace);
}

PHLWORKSPACE COverviewScene::selectedWorkspace() const {
    return m_workspaceTape->selectedWorkspace();
}

PHLWORKSPACE COverviewScene::miniWorkspaceAt(const Vector2D& monitorLocal) const {
    return m_workspaceTape->miniWorkspaceAt(monitorLocal);
}

CBox COverviewScene::mainArea() const {
    return m_layout.logicalMain;
}

bool COverviewScene::pointerMove(const Vector2D& monitorLocal) {
    return m_workspaceSearch->pointerMove(monitorLocal);
}

bool COverviewScene::pointerButton(uint32_t button, bool pressed, const Vector2D& monitorLocal) {
    if (!pressed) {
        const auto TARGET = m_pointerTargets.find(button);
        if (TARGET == m_pointerTargets.end())
            return false;

        const bool HANDLED =
            TARGET->second == ePointerTarget::SEARCH ? m_workspaceSearch->pointerButton(button, false, monitorLocal) : m_workspaceTape->pointerButton(button, false, monitorLocal);
        m_pointerTargets.erase(TARGET);
        return HANDLED;
    }

    if (m_workspaceSearch->pointerButton(button, true, monitorLocal)) {
        m_pointerTargets.insert_or_assign(button, ePointerTarget::SEARCH);
        return true;
    }

    if (!m_workspaceTape->pointerButton(button, true, monitorLocal))
        return false;

    m_pointerTargets.insert_or_assign(button, ePointerTarget::WORKSPACE_TAPE);
    return true;
}

void COverviewScene::pointerLeave() {
    m_workspaceSearch->pointerLeave();
}

void COverviewScene::keyboardKey(uint32_t keysym, bool down, bool repeat, std::string utf8, uint32_t modifiers) {
    m_workspaceSearch->keyboardKey(keysym, down, repeat, std::move(utf8), modifiers);
}

void COverviewScene::reset() {
    m_workspaceSearch->reset();
    m_workspaceTape->reset();
    m_layout = {};
    m_pointerTargets.clear();
}

void COverviewScene::setTextboxFocus(bool x) {
    m_workspaceSearch->setFocused(x);
}
