#include "OverviewScene.hpp"

#include "WorkspaceTapeController.hpp"
#include "WorkspaceSearch.hpp"
#include "WorkspaceSearchController.hpp"
#include "../Overview.hpp"
#include "../Query.hpp"
#include "../../../desktop/Workspace.hpp"
#include "../../../desktop/DesktopTypes.hpp"
#include "../../../desktop/state/WindowState.hpp"
#include "../../../desktop/view/window/Window.hpp"
#include "../../../desktop/view/window/WindowMetadata.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../output/MonitorResources.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../event/EventBus.hpp"
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

static Mode::eWorkspaceMatch workspaceFilter(PHLWORKSPACE ws, const CQuery& query) {
    if (!ws)
        return Mode::eWorkspaceMatch::NONE;

    const auto WORKSPACE_MATCH = query.matchWorkspace(ws->m_name, [ws](std::string_view selector) { return ws->matchesStaticSelector(std::string{selector}); });
    if (WORKSPACE_MATCH != Mode::eWorkspaceMatch::NONE)
        return WORKSPACE_MATCH;

    for (const auto& w : Desktop::windowState()->windows()) {
        if (w->m_workspace != ws)
            continue;

        if (query.matchesWindow(w->metadata().appID(), w->metadata().title()))
            return Mode::eWorkspaceMatch::MATCH;
    }

    return Mode::eWorkspaceMatch::NONE;
}

static char queryPrefix(const std::string& value, char fallback) {
    return value.size() == 1 ? value.front() : fallback;
}

void COverviewScene::start(PHLMONITOR monitor, WP<Monitor::CMonitorResources> resources) {
    m_layout = monitor ? OverviewLayout::calculate(monitor->m_size, monitor->m_scale) : OverviewLayout::SLayout{};
    updateQuery("");
    m_workspaceTape->start(monitor, resources, m_layout);
    m_workspaceSearch->start(monitor, [this](const std::string& query) { updateQuery(query); });
    m_configListener = Event::bus()->m_events.config.props_refreshed.listen([this](bool) { updateQuery(currentQuery()); });
}

void COverviewScene::updateQuery(const std::string& raw) {
    static auto PWINDOWPREFIX    = CConfigValue<Config::STRING>("overview:search:window_prefix");
    static auto PWORKSPACEPREFIX = CConfigValue<Config::STRING>("overview:search:workspace_prefix");
    static auto PDEFAULTMODE     = CConfigValue<Config::INTEGER>("overview:search:default_mode");

    const auto  WINDOW_PREFIX    = *PWINDOWPREFIX;
    const auto  WORKSPACE_PREFIX = *PWORKSPACEPREFIX;

    m_query = makeUnique<CQuery>(
        raw, SQueryConfig{.windowPrefix = queryPrefix(WINDOW_PREFIX, '/'), .workspacePrefix = queryPrefix(WORKSPACE_PREFIX, '.'), .defaultMode = sc<eQueryMode>(*PDEFAULTMODE)});
    m_workspaceTape->setFilter([this](PHLWORKSPACE workspace) { return ::workspaceFilter(workspace, *m_query); }, m_query->usesWindowMetadata());
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

Vector2D COverviewScene::transformPointer(const Vector2D& global) const {
    return m_workspaceTape->transformPointer(global);
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
    m_configListener = {};
    m_workspaceSearch->reset();
    m_workspaceTape->reset();
    m_query.reset();
    m_layout = {};
    m_pointerTargets.clear();
}

void COverviewScene::setTextboxFocus(bool x) {
    m_workspaceSearch->setFocused(x);
}

void COverviewScene::useSelectedWorkspaceForFullscreen(bool x) {
    m_workspaceTape->useSelectedWorkspaceForFullscreen(x);
}

bool COverviewScene::beginMoveGesture() {
    return m_workspaceTape->beginMoveGesture();
}

void COverviewScene::updateMoveGesture(float delta) {
    m_workspaceTape->updateMoveGesture(delta);
}

void COverviewScene::endMoveGesture() {
    m_workspaceTape->endMoveGesture();
}

void COverviewScene::resetQuery() const {
    m_workspaceSearch->resetQuery();
}

std::string COverviewScene::currentQuery() const {
    return m_workspaceSearch->query();
}

const CQuery* COverviewScene::query() const {
    return m_query.get();
}
