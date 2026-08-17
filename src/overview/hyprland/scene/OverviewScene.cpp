#include "OverviewScene.hpp"

#include "../Overview.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../output/MonitorResources.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/pass/ClearPassElement.hpp"
#include "../../../render/pass/TexPassElement.hpp"

#include <algorithm>
#include <cmath>

using namespace Overview::Hyprland;

static constexpr float OVERVIEW_SCALE    = 0.9F;
static constexpr float OVERVIEW_ROUNDING = 20.F;
static constexpr float OVERVIEW_GRAY     = 0.16F;

COverviewScene::COverviewScene(COverview& parent) : m_parent(parent) {
    ;
}

void COverviewScene::draw(Time::steady_tp tp) {
    const auto MONITOR   = m_parent.m_monitor.lock();
    const auto RESOURCES = m_parent.m_resources;
    if (!MONITOR || !RESOURCES || !m_parent.m_progress)
        return;

    if (!m_framebuffer || m_framebuffer->m_size != MONITOR->m_transformedSize)
        m_framebuffer = RESOURCES->getUnusedWorkBuffer();

    if (!m_framebuffer || !g_pHyprRenderer->renderMonitorToBuffer(MONITOR, m_framebuffer, tp)) {
        g_pHyprRenderer->addPassElement(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(OVERVIEW_GRAY, OVERVIEW_GRAY, OVERVIEW_GRAY, 1.F)}));
        return;
    }

    const float    PROGRESS = std::clamp(m_parent.m_progress->value(), 0.F, 1.F);
    const float    SCALE    = 1.F - (1.F - OVERVIEW_SCALE) * PROGRESS;
    const Vector2D SIZE     = MONITOR->m_transformedSize * SCALE;
    const Vector2D POS      = (MONITOR->m_transformedSize - SIZE) / 2.F;

    g_pHyprRenderer->addPassElement(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(OVERVIEW_GRAY, OVERVIEW_GRAY, OVERVIEW_GRAY, 1.F)}));

    CTexPassElement::SRenderData data;
    data.tex   = m_framebuffer->getTexture();
    data.box   = CBox{POS, SIZE};
    data.round = std::lround(OVERVIEW_ROUNDING * MONITOR->m_scale * PROGRESS);
    g_pHyprRenderer->addPassElement(makeUnique<CTexPassElement>(std::move(data)));
}

void COverviewScene::reset() {
    m_framebuffer.reset();
}
