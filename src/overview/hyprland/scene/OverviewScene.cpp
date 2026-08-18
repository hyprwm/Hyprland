#include "OverviewScene.hpp"

#include "WorkspaceTapeController.hpp"
#include "../Overview.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../output/MonitorResources.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/pass/ClearPassElement.hpp"

#include <algorithm>

using namespace Overview::Hyprland;

static constexpr float OVERVIEW_GRAY = 0.16F;

COverviewScene::COverviewScene(COverview& parent) : m_parent(parent), m_workspaceTape(makeUnique<CWorkspaceTapeController>()) {
    ;
}

COverviewScene::~COverviewScene() = default;

void COverviewScene::draw(Time::steady_tp tp) {
    const auto MONITOR = m_parent.m_monitor.lock();
    if (!MONITOR || !m_parent.m_progress || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->addPassElement(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(OVERVIEW_GRAY, OVERVIEW_GRAY, OVERVIEW_GRAY, 1.F)}));
    m_workspaceTape->draw(tp, std::clamp(m_parent.m_progress->value(), 0.F, 1.F));
}

void COverviewScene::start(PHLMONITOR monitor, WP<Monitor::CMonitorResources> resources) {
    m_workspaceTape->start(monitor, resources);
}

bool COverviewScene::navigateLeft() {
    return m_workspaceTape->navigateLeft();
}

bool COverviewScene::navigateRight() {
    return m_workspaceTape->navigateRight();
}

PHLWORKSPACE COverviewScene::selectedWorkspace() const {
    return m_workspaceTape->selectedWorkspace();
}

void COverviewScene::reset() {
    m_workspaceTape->reset();
}
