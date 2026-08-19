#include "OverviewScene.hpp"

#include "WorkspaceTapeController.hpp"
#include "../Overview.hpp"
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

COverviewScene::COverviewScene(COverview& parent) : m_parent(parent), m_workspaceTape(makeUnique<CWorkspaceTapeController>()) {
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
