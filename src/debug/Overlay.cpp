#include "Overlay.hpp"
#include "config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../render/pass/RectPassElement.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../render/Renderer.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../desktop/state/FocusState.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    constexpr float  OVERLAY_REFRESH_INTERVAL_MS = 200.F;
    constexpr int    OVERLAY_MARGIN_TOP          = 4;
    constexpr int    OVERLAY_MARGIN_LEFT         = 4;
    constexpr int    OVERLAY_LINE_GAP            = 1;
    constexpr int    OVERLAY_MONITOR_GAP         = 5;
    constexpr int    OVERLAY_BOX_MARGIN          = 5;

    constexpr int    OVERLAY_FPS_GRAPH_HISTORY_SEC = 30;
    constexpr int    OVERLAY_FPS_GRAPH_BAR_WIDTH   = 3;
    constexpr int    OVERLAY_FPS_GRAPH_BAR_GAP     = 1;
    constexpr int    OVERLAY_FPS_GRAPH_HEIGHT      = 22;
    constexpr int    OVERLAY_FPS_GRAPH_PADDING     = 2;
    constexpr int    OVERLAY_FPS_GRAPH_GAP_TOP     = 2;
    constexpr float  OVERLAY_FPS_GRAPH_BG_ALPHA    = 0.35F;

    const CHyprColor FPS_COLOR_BAD  = CHyprColor{1.F, 0.2F, 0.2F, 1.F};
    const CHyprColor FPS_COLOR_GOOD = CHyprColor{0.2F, 1.F, 0.2F, 1.F};

    struct SFPSGraphLayout {
        float innerWidth  = 0.F;
        float innerHeight = 0.F;
        float width       = 0.F;
        float height      = 0.F;
    };

    struct SFPSGraphDrawResult {
        float width   = 0.F;
        float bottomY = 0.F;
    };
}

static Hyprgraphics::CColor::SOkLab lerp(const Hyprgraphics::CColor::SOkLab& a, const Hyprgraphics::CColor::SOkLab& b, float ratio) {
    return Hyprgraphics::CColor::SOkLab{
        .l = std::lerp(a.l, b.l, ratio),
        .a = std::lerp(a.a, b.a, ratio),
        .b = std::lerp(a.b, b.b, ratio),
    };
}

static CHyprColor fpsBarColor(float normalizedFPS) {
    return CHyprColor{Hyprgraphics::CColor{lerp(FPS_COLOR_BAD.asOkLab(), FPS_COLOR_GOOD.asOkLab(), normalizedFPS)}, 1.F};
}

static SFPSGraphLayout fpsGraphLayout() {
    const float INNERWIDTH  = sc<float>(OVERLAY_FPS_GRAPH_HISTORY_SEC * OVERLAY_FPS_GRAPH_BAR_WIDTH + (OVERLAY_FPS_GRAPH_HISTORY_SEC - 1) * OVERLAY_FPS_GRAPH_BAR_GAP);
    const float INNERHEIGHT = sc<float>(OVERLAY_FPS_GRAPH_HEIGHT);

    return {
        .innerWidth  = INNERWIDTH,
        .innerHeight = INNERHEIGHT,
        .width       = INNERWIDTH + OVERLAY_FPS_GRAPH_PADDING * 2.F,
        .height      = INNERHEIGHT + OVERLAY_FPS_GRAPH_PADDING * 2.F,
    };
}

static SFPSGraphDrawResult drawFPSGraph(float x, float y, float idealFPS, const std::deque<float>& fpsHistory) {
    const auto                  LAYOUT = fpsGraphLayout();

    CRectPassElement::SRectData bgData;
    bgData.box   = {x, y, LAYOUT.width, LAYOUT.height};
    bgData.color = CHyprColor{0.F, 0.F, 0.F, OVERLAY_FPS_GRAPH_BG_ALPHA};
    bgData.round = 2;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(bgData));

    const size_t BARCOUNT         = std::min(fpsHistory.size(), sc<size_t>(OVERLAY_FPS_GRAPH_HISTORY_SEC));
    const size_t LEADINGBLANKBARS = sc<size_t>(OVERLAY_FPS_GRAPH_HISTORY_SEC) - BARCOUNT;

    for (size_t bar = 0; bar < BARCOUNT; ++bar) {
        const float                 FPSVALUE      = fpsHistory[fpsHistory.size() - BARCOUNT + bar];
        const float                 NORMALIZEDFPS = std::clamp(FPSVALUE / idealFPS, 0.F, 1.F);
        const float                 BARHEIGHT     = std::max(1.F, std::round(NORMALIZEDFPS * LAYOUT.innerHeight));
        const float                 BARX          = x + OVERLAY_FPS_GRAPH_PADDING + sc<float>((LEADINGBLANKBARS + bar) * (OVERLAY_FPS_GRAPH_BAR_WIDTH + OVERLAY_FPS_GRAPH_BAR_GAP));
        const float                 BARY          = y + OVERLAY_FPS_GRAPH_PADDING + (LAYOUT.innerHeight - BARHEIGHT);

        CRectPassElement::SRectData barData;
        barData.box   = {BARX, BARY, sc<float>(OVERLAY_FPS_GRAPH_BAR_WIDTH), BARHEIGHT};
        barData.color = fpsBarColor(NORMALIZEDFPS);
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(barData));
    }

    return {
        .width   = LAYOUT.width,
        .bottomY = y + LAYOUT.height,
    };
}

using namespace Debug;

UP<COverlay>& Debug::overlay() {
    static UP<COverlay> p = makeUnique<COverlay>();
    return p;
}

COverlay::COverlay() {
    m_frameTimer.reset();
}

void CMonitorOverlay::renderData(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Config::INTEGER>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitor = pMonitor;

    m_lastRenderTimes.emplace_back(durationUs / 1000.F);

    const auto SAMPLELIMIT = std::max<size_t>(1, sc<size_t>(std::ceil(std::max(1.F, pMonitor->m_refreshRate))));

    if (m_lastRenderTimes.size() > SAMPLELIMIT)
        m_lastRenderTimes.pop_front();
}

void CMonitorOverlay::renderDataNoOverlay(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Config::INTEGER>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitor = pMonitor;

    m_lastRenderTimesNoOverlay.emplace_back(durationUs / 1000.F);

    const auto SAMPLELIMIT = std::max<size_t>(1, sc<size_t>(std::ceil(std::max(1.F, pMonitor->m_refreshRate))));

    if (m_lastRenderTimesNoOverlay.size() > SAMPLELIMIT)
        m_lastRenderTimesNoOverlay.pop_front();
}

void CMonitorOverlay::frameData(PHLMONITOR pMonitor) {
    static auto PDEBUGOVERLAY = CConfigValue<Config::INTEGER>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitor = pMonitor;

    const auto NOW = std::chrono::high_resolution_clock::now();
    if (m_lastFrame.time_since_epoch().count() != 0)
        m_lastFrametimes.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(NOW - m_lastFrame).count() / 1000.F);

    const auto SAMPLELIMIT = std::max<size_t>(1, sc<size_t>(std::ceil(std::max(1.F, pMonitor->m_refreshRate))));

    if (m_lastFrametimes.size() > SAMPLELIMIT)
        m_lastFrametimes.pop_front();

    m_lastFrame = NOW;

    if (m_fpsSecondStart.time_since_epoch().count() == 0)
        m_fpsSecondStart = NOW;

    ++m_framesInCurrentSecond;

    const auto SECONDWINDOWMS = std::chrono::duration_cast<std::chrono::milliseconds>(NOW - m_fpsSecondStart).count();
    if (SECONDWINDOWMS >= 1000) {
        const float ELAPSEDSECONDS = SECONDWINDOWMS / 1000.F;
        const float IDEALFPS       = std::max(1.F, pMonitor->m_refreshRate);
        const float FPSINWINDOW    = ELAPSEDSECONDS > 0.F ? m_framesInCurrentSecond / ELAPSEDSECONDS : 0.F;

        m_lastFPSPerSecond.emplace_back(std::clamp(FPSINWINDOW, 0.F, IDEALFPS));

        if (m_lastFPSPerSecond.size() > OVERLAY_FPS_GRAPH_HISTORY_SEC)
            m_lastFPSPerSecond.pop_front();

        m_framesInCurrentSecond = 0;
        m_fpsSecondStart        = NOW;
    }

    // anim data too
    const auto PMONITORFORTICKS = g_pHyprRenderer->m_mostHzMonitor ? g_pHyprRenderer->m_mostHzMonitor.lock() : Desktop::focusState()->monitor();
    if (PMONITORFORTICKS && PMONITORFORTICKS == pMonitor) {
        const auto TICKLIMIT = std::max<size_t>(1, sc<size_t>(std::ceil(std::max(1.F, PMONITORFORTICKS->m_refreshRate))));

        if (m_lastAnimationTicks.size() > TICKLIMIT)
            m_lastAnimationTicks.pop_front();

        m_lastAnimationTicks.push_back(g_pAnimationManager->m_lastTickTimeMs);
    }
}

const CBox& CMonitorOverlay::lastDrawnBox() const {
    return m_lastDrawnBox;
}

void CMonitorOverlay::updateLine(size_t idx, const std::string& text, const CHyprColor& color, int fontSize, const std::string& fontFamily) {
    if (m_cachedLines.size() <= idx)
        m_cachedLines.resize(idx + 1);

    auto& line = m_cachedLines[idx];
    if (line.texture && line.text == text && line.fontSize == fontSize && line.color == color)
        return;

    line.text     = text;
    line.color    = color;
    line.fontSize = fontSize;
    line.texture  = g_pHyprRenderer->renderText(text, color, fontSize, false, fontFamily);
}

void CMonitorOverlay::rebuildCache() {
    m_cachedLines.clear();

    if (!m_monitor)
        return;

    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR)
        return;

    auto metricsFromSamples = [](const std::deque<float>& samples) -> SMetricData {
        SMetricData metric;

        if (samples.empty())
            return metric;

        metric.min = std::numeric_limits<float>::max();
        metric.max = std::numeric_limits<float>::lowest();

        for (const auto sample : samples) {
            metric.avg += sample;
            metric.min = std::min(metric.min, sample);
            metric.max = std::max(metric.max, sample);
        }

        metric.avg /= samples.size();
        metric.var = metric.max - metric.min;

        return metric;
    };

    const auto  FRAMEMETRICS         = metricsFromSamples(m_lastFrametimes);
    const auto  RENDERMETRICS        = metricsFromSamples(m_lastRenderTimes);
    const auto  RENDERMETRICSNOOVL   = metricsFromSamples(m_lastRenderTimesNoOverlay);
    const auto  ANIMATIONTICKMETRICS = metricsFromSamples(m_lastAnimationTicks);

    const float FPS      = FRAMEMETRICS.avg <= 0.F ? 0.F : 1000.F / FRAMEMETRICS.avg;
    const float IDEALFPS = std::max(1.F, PMONITOR->m_refreshRate);
    const float TICKTPS  = ANIMATIONTICKMETRICS.avg <= 0.F ? 0.F : 1000.F / ANIMATIONTICKMETRICS.avg;

    static auto FONTFAMILY = CConfigValue<Config::STRING>("misc:font_family");

    CHyprColor  fpsColor = CHyprColor{1.F, 0.2F, 0.2F, 1.F};
    if (FPS > IDEALFPS * 0.95F)
        fpsColor = CHyprColor{0.2F, 1.F, 0.2F, 1.F};
    else if (FPS > IDEALFPS * 0.8F)
        fpsColor = CHyprColor{1.F, 1.F, 0.2F, 1.F};

    size_t idx = 0;
    updateLine(idx++, PMONITOR->m_name, CHyprColor{1.F, 1.F, 1.F, 1.F}, 10, *FONTFAMILY);
    updateLine(idx++, std::format("{} FPS", sc<int>(std::round(FPS))), fpsColor, 16, *FONTFAMILY);
    updateLine(idx++, std::format("Avg Frametime: {:.2f}ms (var {:.2f}ms)", FRAMEMETRICS.avg, FRAMEMETRICS.var), CHyprColor{1.F, 1.F, 1.F, 1.F}, 10, *FONTFAMILY);
    updateLine(idx++, std::format("Avg Rendertime: {:.2f}ms (var {:.2f}ms)", RENDERMETRICS.avg, RENDERMETRICS.var), CHyprColor{1.F, 1.F, 1.F, 1.F}, 10, *FONTFAMILY);
    updateLine(idx++, std::format("Avg Rendertime (No Overlay): {:.2f}ms (var {:.2f}ms)", RENDERMETRICSNOOVL.avg, RENDERMETRICSNOOVL.var), CHyprColor{1.F, 1.F, 1.F, 1.F}, 10,
               *FONTFAMILY);
    updateLine(idx++, std::format("Avg Anim Tick: {:.2f}ms (var {:.2f}ms) ({:.2f} TPS)", ANIMATIONTICKMETRICS.avg, ANIMATIONTICKMETRICS.var, TICKTPS),
               CHyprColor{1.F, 1.F, 1.F, 1.F}, 10, *FONTFAMILY);

    m_cachedLines.resize(idx);
}

int CMonitorOverlay::draw(int offset, bool& cacheUpdated) {
    cacheUpdated   = false;
    m_lastDrawnBox = {};

    if (!m_monitor)
        return 0;

    if (!m_cacheValid || m_cacheTimer.getMillis() >= OVERLAY_REFRESH_INTERVAL_MS) {
        rebuildCache();
        m_cacheValid = true;
        m_cacheTimer.reset();
        cacheUpdated = true;
    }

    const auto  PMONITOR = m_monitor.lock();
    const float IDEALFPS = PMONITOR ? std::max(1.F, PMONITOR->m_refreshRate) : 1.F;

    float       y        = offset + OVERLAY_MARGIN_TOP + OVERLAY_BOX_MARGIN;
    float       maxTextW = 0.F;

    for (size_t i = 0; i < m_cachedLines.size(); ++i) {
        const auto& line = m_cachedLines[i];
        if (!line.texture)
            continue;

        CTexPassElement::SRenderData data;
        data.tex = line.texture;
        data.box = {OVERLAY_MARGIN_LEFT + OVERLAY_BOX_MARGIN, y, line.texture->m_size.x, line.texture->m_size.y};
        data.a   = 1.F;
        g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));

        maxTextW = std::max(maxTextW, sc<float>(line.texture->m_size.x));
        y += line.texture->m_size.y + OVERLAY_LINE_GAP;

        if (i == 1) {
            const auto GRAPHDRAW = drawFPSGraph(OVERLAY_MARGIN_LEFT + OVERLAY_BOX_MARGIN, y + OVERLAY_FPS_GRAPH_GAP_TOP, IDEALFPS, m_lastFPSPerSecond);

            maxTextW = std::max(maxTextW, GRAPHDRAW.width);
            y        = GRAPHDRAW.bottomY + OVERLAY_LINE_GAP;
        }
    }

    const float HEIGHT = y - offset - OVERLAY_MARGIN_TOP - OVERLAY_BOX_MARGIN;
    if (maxTextW <= 0.F || HEIGHT <= 0.F)
        return 0;

    m_lastDrawnBox = {OVERLAY_MARGIN_LEFT - 1 + OVERLAY_BOX_MARGIN, offset + OVERLAY_MARGIN_TOP + OVERLAY_BOX_MARGIN - 1, sc<int>(std::ceil(maxTextW)) + 2,
                      sc<int>(std::ceil(HEIGHT)) + 2};
    return sc<int>(std::ceil(y - offset));
}

Vector2D CMonitorOverlay::size() const {
    return m_lastDrawnBox.size(); // this shouldn't change much
}

void COverlay::renderData(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Config::INTEGER>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitorOverlays[pMonitor].renderData(pMonitor, durationUs);
}

void COverlay::renderDataNoOverlay(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Config::INTEGER>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitorOverlays[pMonitor].renderDataNoOverlay(pMonitor, durationUs);
}

void COverlay::frameData(PHLMONITOR pMonitor) {
    static auto PDEBUGOVERLAY = CConfigValue<Config::INTEGER>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitorOverlays[pMonitor].frameData(pMonitor);
}

void COverlay::createWarningTexture(float maxW) {
    if (maxW <= 1) {
        m_warningTexture.reset();
        m_warningTextureMaxW = 0;
        return;
    }

    if (maxW == m_warningTextureMaxW)
        return;

    static auto FONT = CConfigValue<Config::STRING>("misc:font_family");

    m_warningTexture = g_pHyprRenderer->renderText(Hyprgraphics::CTextResource::STextResourceData{
        .text     = "[!] FPS might be below your monitor's refresh rate if there are no content updates",
        .font     = *FONT,
        .fontSize = 8,
        .color    = Colors::YELLOW.asRGB(),
        .maxSize  = Vector2D{maxW, -1.F},
    });
}

void COverlay::draw() {
    if (g_pCompositor->m_monitors.empty())
        return;

    const auto PMONITOR = g_pCompositor->m_monitors.front();
    if (!PMONITOR)
        return;

    bool  haveAnyBox   = false;
    bool  cacheUpdated = false;
    int   minX         = 0;
    int   minY         = 0;
    int   maxX         = 0;
    int   maxY         = 0;
    int   offsetY      = 0;

    float maxWidth = 0;

    // draw background first
    {
        Vector2D fullSize                = {};
        int      monitorsWithOverlayData = 0;

        for (const auto& m : g_pCompositor->m_monitors) {
            const Vector2D size = m_monitorOverlays[m].size();
            if (size.x <= 0 || size.y <= 0)
                continue;

            fullSize.y += size.y + OVERLAY_MONITOR_GAP;
            fullSize.x = std::max(fullSize.x, size.x);
            ++monitorsWithOverlayData;
        }

        if (monitorsWithOverlayData > 0) {
            fullSize.y -= OVERLAY_MONITOR_GAP;

            // Each monitor section is offset by OVERLAY_MARGIN_TOP + OVERLAY_BOX_MARGIN in CMonitorOverlay::draw,
            // while the per-monitor drawn box height only tracks content (+2 px padding).
            // Account for that inter-section offset so the backdrop spans stacked monitor overlays correctly.
            fullSize.y += sc<float>((monitorsWithOverlayData - 1) * (OVERLAY_MARGIN_TOP + OVERLAY_BOX_MARGIN - 2));
        }

        maxWidth = fullSize.x;

        if (fullSize.y > 1 && fullSize.x > 1) {
            CRectPassElement::SRectData data;
            data.box   = CBox{{OVERLAY_MARGIN_LEFT, OVERLAY_MARGIN_TOP}, fullSize + Vector2D{OVERLAY_BOX_MARGIN, OVERLAY_BOX_MARGIN} * 2.F};
            data.color = CHyprColor{0.1F, 0.1F, 0.1F, 0.6F};
            data.round = 10;
            data.blur  = true;
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(std::move(data)));

            createWarningTexture(fullSize.x);
        }
    }

    for (auto const& monitor : g_pCompositor->m_monitors) {
        bool monitorUpdated = false;
        offsetY += m_monitorOverlays[monitor].draw(offsetY, monitorUpdated);
        cacheUpdated = cacheUpdated || monitorUpdated;

        const auto& BOX = m_monitorOverlays[monitor].lastDrawnBox();
        if (BOX.width > 0 && BOX.height > 0) {
            const int boxMinX = sc<int>(std::floor(BOX.x));
            const int boxMinY = sc<int>(std::floor(BOX.y));
            const int boxMaxX = sc<int>(std::ceil(BOX.x + BOX.width));
            const int boxMaxY = sc<int>(std::ceil(BOX.y + BOX.height));

            if (!haveAnyBox) {
                minX       = boxMinX;
                minY       = boxMinY;
                maxX       = boxMaxX;
                maxY       = boxMaxY;
                haveAnyBox = true;
            } else {
                minX = std::min(minX, boxMinX);
                minY = std::min(minY, boxMinY);
                maxX = std::max(maxX, boxMaxX);
                maxY = std::max(maxY, boxMaxY);
            }
        }

        offsetY += OVERLAY_MONITOR_GAP;
    }

    offsetY -= OVERLAY_MONITOR_GAP;

    // render warning texture
    if (m_warningTexture) {
        {
            CRectPassElement::SRectData data;
            data.box   = CBox{{OVERLAY_MARGIN_LEFT, offsetY + (OVERLAY_MARGIN_TOP * 2) + OVERLAY_BOX_MARGIN},
                              {maxWidth + (OVERLAY_BOX_MARGIN * 2.F), m_warningTexture->m_size.y + (OVERLAY_BOX_MARGIN * 2.F)}};
            data.color = CHyprColor{0.1F, 0.1F, 0.1F, 0.6F};
            data.round = 10;
            data.blur  = true;
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(std::move(data)));
        }

        {
            CTexPassElement::SRenderData data;
            data.box = CBox{
                Vector2D{OVERLAY_MARGIN_LEFT + ((maxWidth - m_warningTexture->m_size.x) / 2.F), sc<float>(offsetY) + (OVERLAY_MARGIN_TOP * 2) + (OVERLAY_BOX_MARGIN * 2)}.round(),
                m_warningTexture->m_size};
            data.tex = m_warningTexture;
            g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
        }
    }

    CBox newDrawnBox;
    if (haveAnyBox)
        newDrawnBox = {sc<int>(PMONITOR->m_position.x) + minX, sc<int>(PMONITOR->m_position.y) + minY, maxX - minX, maxY - minY};

    if (cacheUpdated || newDrawnBox != m_lastDrawnBox) {
        if (m_lastDrawnBox.width > 0 && m_lastDrawnBox.height > 0)
            g_pHyprRenderer->damageBox(m_lastDrawnBox);

        if (newDrawnBox.width > 0 && newDrawnBox.height > 0)
            g_pHyprRenderer->damageBox(newDrawnBox);

        m_lastDrawnBox = newDrawnBox;
    }

    if (m_frameTimer.getMillis() >= OVERLAY_REFRESH_INTERVAL_MS) {
        g_pCompositor->scheduleFrameForMonitor(PMONITOR);
        m_frameTimer.reset();
    }
}
