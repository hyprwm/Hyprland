#include "Overlay.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/shared/animation/AnimationTree.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../event/EventBus.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../render/Renderer.hpp"
#include "../render/pass/BorderPassElement.hpp"
#include "../render/pass/RectPassElement.hpp"
#include "../render/pass/TexPassElement.hpp"

#include <algorithm>
#include <format>

using namespace Hyprutils::Animation;
using namespace ErrorOverlay;

static std::string takeFirstNLines(const std::string& text, size_t lines) {
    if (lines <= 0)
        return "";

    size_t begin = 0;
    size_t count = 1;
    while (count < lines) {
        const auto nlPos = text.find('\n', begin);
        if (nlPos == std::string::npos)
            return text;

        begin = nlPos + 1;
        ++count;
    }

    const auto cutPos = text.find('\n', begin);
    return cutPos == std::string::npos ? text : text.substr(0, cutPos);
}

static std::string buildVisibleText(const std::string& text, size_t lineLimit) {
    const size_t lineCount    = 1 + std::ranges::count(text, '\n');
    const size_t visibleLines = std::max<size_t>(0, std::min(lineCount, lineLimit));

    std::string  visibleText = takeFirstNLines(text, visibleLines);
    if (visibleLines < lineCount) {
        if (!visibleText.empty())
            visibleText += '\n';

        visibleText += std::format("({} more...)", lineCount - visibleLines);
    }

    return visibleText;
}

UP<COverlay>& ErrorOverlay::overlay() {
    static UP<COverlay> p = makeUnique<COverlay>();
    return p;
}

COverlay::COverlay() {
    g_pAnimationManager->createAnimation(0.f, m_fadeOpacity, Config::animationTree()->getAnimationPropertyConfig("fadeIn"), AVARDAMAGE_NONE);

    static auto P = Event::bus()->m_events.monitor.focused.listen([&](PHLMONITOR mon) {
        if (!m_isCreated)
            return;

        g_pHyprRenderer->damageMonitor(Desktop::focusState()->monitor());
        m_monitorChanged = true;
    });

    static auto P2 = Event::bus()->m_events.render.pre.listen([&](PHLMONITOR mon) {
        if (!m_isCreated)
            return;

        if (m_fadeOpacity->isBeingAnimated() || m_monitorChanged)
            g_pHyprRenderer->damageBox(m_damageBox);
    });
}

void COverlay::queueCreate(std::string message, const CHyprColor& color) {
    queueCreate(std::move(message), Config::CGradientValueData{color});
}

void COverlay::queueCreate(std::string message, const Config::CGradientValueData& gradient) {
    m_queued               = std::move(message);
    m_queuedBorderGradient = gradient;

    if (m_queuedBorderGradient.m_colors.empty())
        m_queuedBorderGradient.reset(CHyprColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
    else
        m_queuedBorderGradient.updateColorsOk();
}

void COverlay::queueError(std::string err) {
    queueCreate(err + "\nHyprland may not work correctly.", CHyprColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
}

void COverlay::createQueued() {
    if (m_isCreated && m_textTexture)
        m_textTexture.reset();

    m_fadeOpacity->setConfig(Config::animationTree()->getAnimationPropertyConfig("fadeIn"));
    m_fadeOpacity->setValueAndWarp(0.f);
    *m_fadeOpacity = 1.f;

    const auto PMONITOR = g_pCompositor->m_monitors.front();
    if (!PMONITOR)
        return;

    const float       SCALE    = PMONITOR->m_scale;
    const int         FONTSIZE = std::clamp(sc<int>(10.f * ((PMONITOR->m_pixelSize.x * SCALE) / 1920.f)), 8, 40);

    static auto       LINELIMIT    = CConfigValue<Hyprlang::INT>("debug:error_limit");
    static auto       BAR_POSITION = CConfigValue<Hyprlang::INT>("debug:error_position");
    static auto       FONT_FAMILY  = CConfigValue<std::string>("misc:font_family");

    const bool        TOPBAR      = *BAR_POSITION == 0;
    const std::string visibleText = buildVisibleText(m_queued, *LINELIMIT);

    m_outerPad = 10.F * SCALE;

    const float barWidth     = std::max<float>(1.F, sc<float>(PMONITOR->m_pixelSize.x) - m_outerPad * 2.F);
    const float textMaxWidth = std::max<float>(1.F, barWidth - 2.F * (1.F + m_outerPad));

    m_textTexture = g_pHyprRenderer->renderText(Hyprgraphics::CTextResource::STextResourceData{
        .text      = visibleText,
        .font      = *FONT_FAMILY,
        .fontSize  = sc<size_t>(FONTSIZE),
        .color     = CHyprColor(0.9, 0.9, 0.9, 1.0).asRGB(),
        .maxSize   = Vector2D{textMaxWidth, -1.F},
        .ellipsize = false,
        .wrap      = true,
    });

    m_textSize = m_textTexture ? m_textTexture->m_size : Vector2D{};

    m_lastHeight  = std::max<float>(3.F, sc<float>(m_textSize.y) + 3.F);
    m_radius      = std::min<float>(m_outerPad, std::max<float>(0.F, m_lastHeight / 2.F - 1.F));
    m_textOffsetX = 1.F + m_radius;
    m_textOffsetY = 1.F;

    m_damageBox = {
        sc<int>(PMONITOR->m_position.x),
        sc<int>(PMONITOR->m_position.y + (TOPBAR ? 0 : PMONITOR->m_pixelSize.y - (m_lastHeight + m_outerPad * 2.F))),
        sc<int>(PMONITOR->m_pixelSize.x),
        sc<int>(m_lastHeight + m_outerPad * 2.F),
    };

    m_borderGradient = m_queuedBorderGradient;

    m_isCreated     = true;
    m_queued        = "";
    m_queuedDestroy = false;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    for (const auto& m : g_pCompositor->m_monitors) {
        m->m_reservedArea.resetType(Desktop::RESERVED_DYNAMIC_TYPE_ERROR_BAR);
    }

    const auto RESERVED = (m_lastHeight + m_outerPad) / SCALE;
    PMONITOR->m_reservedArea.addType(Desktop::RESERVED_DYNAMIC_TYPE_ERROR_BAR, Vector2D{0.0, TOPBAR ? RESERVED : 0.0}, Vector2D{0.0, !TOPBAR ? RESERVED : 0.0});

    for (const auto& m : g_pCompositor->m_monitors) {
        g_pHyprRenderer->arrangeLayersForMonitor(m->m_id);
    }
}

void COverlay::draw() {
    if (!m_isCreated || !m_queued.empty()) {
        if (!m_queued.empty())
            createQueued();
        return;
    }

    if (m_queuedDestroy) {
        if (!m_fadeOpacity->isBeingAnimated()) {
            if (m_fadeOpacity->value() == 0.f) {
                m_queuedDestroy = false;
                m_textTexture.reset();
                m_textSize  = {};
                m_outerPad  = 0.F;
                m_radius    = 0.F;
                m_isCreated = false;
                m_queued    = "";

                for (auto& m : g_pCompositor->m_monitors) {
                    g_pHyprRenderer->arrangeLayersForMonitor(m->m_id);
                    m->m_reservedArea.resetType(Desktop::RESERVED_DYNAMIC_TYPE_ERROR_BAR);
                }

                return;
            } else {
                m_fadeOpacity->setConfig(Config::animationTree()->getAnimationPropertyConfig("fadeOut"));
                *m_fadeOpacity = 0.f;
            }
        }
    }

    const auto PMONITOR = g_pHyprRenderer->m_renderData.pMonitor;
    if (!PMONITOR)
        return;

    static auto BAR_POSITION = CConfigValue<Hyprlang::INT>("debug:error_position");
    const bool  TOPBAR       = *BAR_POSITION == 0;

    const float barWidth = std::max<float>(1.F, sc<float>(PMONITOR->m_pixelSize.x) - m_outerPad * 2.F);
    const float barY     = TOPBAR ? m_outerPad : PMONITOR->m_pixelSize.y - m_lastHeight - m_outerPad;
    const CBox  barBox   = {m_outerPad, barY, barWidth, m_lastHeight};

    m_damageBox.x      = sc<int>(PMONITOR->m_position.x);
    m_damageBox.width  = sc<int>(PMONITOR->m_pixelSize.x);
    m_damageBox.height = sc<int>(m_lastHeight + m_outerPad * 2.F);
    m_damageBox.y      = sc<int>(PMONITOR->m_position.y + (TOPBAR ? 0 : PMONITOR->m_pixelSize.y - m_damageBox.height));

    if (m_fadeOpacity->isBeingAnimated() || m_monitorChanged)
        g_pHyprRenderer->damageBox(m_damageBox);

    m_monitorChanged = false;

    const float                 opacity = m_fadeOpacity->value();

    CRectPassElement::SRectData bgData;
    bgData.box   = barBox;
    bgData.color = CHyprColor(0.06, 0.06, 0.06, opacity);
    bgData.round = sc<int>(std::round(m_radius));
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(std::move(bgData)));

    CBorderPassElement::SBorderData borderData;
    borderData.box        = barBox;
    borderData.grad1      = m_borderGradient;
    borderData.round      = sc<int>(std::round(m_radius));
    borderData.outerRound = sc<int>(std::round(m_radius));
    borderData.borderSize = 2;
    borderData.a          = opacity;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(std::move(borderData)));

    if (m_textTexture) {
        CTexPassElement::SRenderData textData;
        textData.tex        = m_textTexture;
        textData.box        = {Vector2D{barBox.x + m_textOffsetX, barBox.y + m_textOffsetY}.round(), m_textSize};
        textData.a          = opacity;
        textData.clipRegion = CRegion(barBox.copy().expand(-1));

        g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(textData)));
    }
}

void COverlay::destroy() {
    if (m_isCreated)
        m_queuedDestroy = true;
    else
        m_queued = "";
}

bool COverlay::active() {
    return m_isCreated;
}

float COverlay::height() {
    return m_lastHeight;
}
