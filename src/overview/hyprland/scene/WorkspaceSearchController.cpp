#include "WorkspaceSearchController.hpp"

#include "WorkspaceSearch.hpp"
#include "../../../hyprtoolkit/embedded/EmbeddedToolkitManager.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../pointer/cursor/CursorShapeOverrideController.hpp"
#include "../../../render/Framebuffer.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/pass/TexPassElement.hpp"

#include <algorithm>
#include <cmath>

#include <hyprtoolkit/core/Input.hpp>
#include <hyprtoolkit/element/Element.hpp>
#include <hyprtoolkit/element/Textbox.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprtoolkit/types/PointerShape.hpp>
#include <hyprtoolkit/window/EmbeddedSurface.hpp>
#include <linux/input-event-codes.h>

using namespace Overview::Hyprland;

static std::string cursorName(Hyprtoolkit::ePointerShape shape) {
    switch (shape) {
        case Hyprtoolkit::HT_POINTER_POINTER: return "pointer";
        case Hyprtoolkit::HT_POINTER_TEXT: return "text";
        case Hyprtoolkit::HT_POINTER_RESIZE_NS: return "ns-resize";
        case Hyprtoolkit::HT_POINTER_RESIZE_EW: return "ew-resize";
        case Hyprtoolkit::HT_POINTER_RESIZE_NESW: return "nesw-resize";
        case Hyprtoolkit::HT_POINTER_RESIZE_NWSE: return "nwse-resize";
        default: return "default";
    }
}

CWorkspaceSearchController::CWorkspaceSearchController() = default;

CWorkspaceSearchController::~CWorkspaceSearchController() {
    reset();
}

void CWorkspaceSearchController::start(PHLMONITOR monitor, FTextChanged textChanged) {
    reset();
    if (!monitor || !EmbeddedToolkit::manager())
        return;

    m_monitor     = monitor;
    m_textChanged = std::move(textChanged);
    m_surface     = EmbeddedToolkit::manager()->createSurface();
    if (!m_surface)
        return;

    m_textbox = Hyprtoolkit::CTextboxBuilder::begin()
                    ->placeholder(std::string{"Search..."})
                    ->defaultText(std::string{})
                    ->multiline(false)
                    ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                    ->onTextEdited([this](SP<Hyprtoolkit::CTextboxElement>, const std::string& text) {
                        if (m_textChanged)
                            m_textChanged(text);
                    })
                    ->commence();
    m_textbox->setOpacity(0.F);
    m_textbox->animateOpacity(Hyprtoolkit::AnimationPresets::Medium);
    m_surface->rootElement()->addChild(m_textbox);

    m_listeners.frameRequested = m_surface->m_events.frameRequested.listen([this] {
        m_needsFrame = true;
        damageMonitor();
    });
    m_listeners.damaged        = m_surface->m_events.damaged.listen([this](auto) {
        m_needsFrame = true;
        damageMonitor();
    });
    m_listeners.cursorChanged  = m_surface->m_events.cursorChanged.listen(
        [](Hyprtoolkit::ePointerShape shape) { Pointer::Cursor::overrideController->setOverride(cursorName(shape), Pointer::Cursor::CURSOR_OVERRIDE_INTERNAL_UI); });

    updateGeometry();
}

void CWorkspaceSearchController::reset() {
    pointerLeave();
    m_listeners = {};
    m_textbox.reset();
    m_surface.reset();
    m_framebuffer.reset();
    m_monitor.reset();
    m_textChanged    = {};
    m_logicalBox     = {};
    m_pixelBox       = {};
    m_pressedButtons = 0;
    m_needsFrame     = true;
    m_fullRedraw     = true;
}

void CWorkspaceSearchController::draw(Render::CRenderingContext& context, float opacity) {
    const auto MONITOR = m_monitor.lock();
    if (!MONITOR || !m_surface || !g_pHyprRenderer)
        return;

    updateGeometry();
    if (m_pixelBox.empty())
        return;

    const Vector2D BUFFER_SIZE = m_pixelBox.size();
    if (!m_framebuffer)
        m_framebuffer = g_pHyprRenderer->createFB("overview-search");
    if (!m_framebuffer || !m_framebuffer->alloc(std::lround(BUFFER_SIZE.x), std::lround(BUFFER_SIZE.y)))
        return;

    if (m_needsFrame) {
        auto guard   = g_pHyprRenderer->bindTempFB(context, m_framebuffer);
        m_needsFrame = false;
        m_surface->render(m_fullRedraw ? 0 : 1);
        m_fullRedraw = false;
    }

    CTexPassElement::SRenderData data;
    data.tex     = m_framebuffer->getTexture();
    data.box     = m_pixelBox;
    data.a       = std::clamp(opacity, 0.F, 1.F);
    data.clipBox = {{}, MONITOR->m_transformedSize};
    g_pHyprRenderer->addPassElement(context, makeUnique<CTexPassElement>(std::move(data)));
}

bool CWorkspaceSearchController::pointerMove(const Vector2D& monitorLocal) {
    if (!m_surface || !contains(monitorLocal)) {
        if (m_surface && m_pointerEntered && m_pressedButtons > 0) {
            m_surface->pointerMotion(monitorLocal - m_logicalBox.pos());
            return true;
        }

        pointerLeave();
        return false;
    }

    const auto LOCAL = monitorLocal - m_logicalBox.pos();
    if (!m_pointerEntered) {
        m_surface->pointerEnter(LOCAL);
        m_pointerEntered = true;
    } else
        m_surface->pointerMotion(LOCAL);
    return true;
}

bool CWorkspaceSearchController::pointerButton(uint32_t button, bool pressed, const Vector2D& monitorLocal) {
    if (!m_surface || (pressed && !m_pointerEntered && !pointerMove(monitorLocal)))
        return false;

    Hyprtoolkit::Input::eMouseButton toolkitButton = Hyprtoolkit::Input::MOUSE_BUTTON_UNKNOWN;
    if (button == BTN_LEFT)
        toolkitButton = Hyprtoolkit::Input::MOUSE_BUTTON_LEFT;
    else if (button == BTN_RIGHT)
        toolkitButton = Hyprtoolkit::Input::MOUSE_BUTTON_RIGHT;
    else if (button == BTN_MIDDLE)
        toolkitButton = Hyprtoolkit::Input::MOUSE_BUTTON_MIDDLE;

    m_surface->pointerButton(toolkitButton, pressed);
    if (pressed)
        ++m_pressedButtons;
    else if (m_pressedButtons > 0)
        --m_pressedButtons;

    if (!pressed && m_pressedButtons == 0 && !contains(monitorLocal))
        pointerLeave();
    return true;
}

void CWorkspaceSearchController::pointerLeave() {
    if (m_surface && m_pointerEntered)
        m_surface->pointerLeave();
    Pointer::Cursor::overrideController->unsetOverride(Pointer::Cursor::CURSOR_OVERRIDE_INTERNAL_UI);
    m_pointerEntered = false;
}

void CWorkspaceSearchController::keyboardKey(uint32_t keysym, bool down, bool repeat, std::string utf8, uint32_t modifiers) {
    if (!m_surface)
        return;

    m_surface->keyboardKey({
        .xkbKeysym = keysym,
        .down      = down,
        .repeat    = repeat,
        .utf8      = std::move(utf8),
        .modMask   = modifiers,
    });
}

bool CWorkspaceSearchController::contains(const Vector2D& monitorLocal) const {
    return !m_logicalBox.empty() && m_logicalBox.containsPoint(monitorLocal);
}

std::string CWorkspaceSearchController::query() const {
    return m_textbox ? std::string{m_textbox->currentText()} : std::string{};
}

void CWorkspaceSearchController::updateGeometry() {
    const auto MONITOR = m_monitor.lock();
    if (!MONITOR || !m_surface)
        return;

    const auto GEOMETRY = WorkspaceSearch::calculateGeometry(MONITOR->m_size, MONITOR->m_scale);
    if (GEOMETRY.logicalBox == m_logicalBox && GEOMETRY.pixelBox == m_pixelBox)
        return;

    m_logicalBox = GEOMETRY.logicalBox;
    m_pixelBox   = GEOMETRY.pixelBox;
    if (m_logicalBox.empty() || m_pixelBox.empty())
        return;

    m_surface->resize(m_logicalBox.size(), m_pixelBox.size(), MONITOR->m_scale);
    m_framebuffer.reset();
    m_needsFrame = true;
    m_fullRedraw = true;
}

void CWorkspaceSearchController::damageMonitor() const {
    if (const auto MONITOR = m_monitor.lock(); MONITOR && g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

void CWorkspaceSearchController::setFocused(bool x) {
    if (m_focused == x)
        return;

    m_focused = x;

    if (m_textbox) {
        m_textbox->setOpacity(x ? 1.F : 0.F);
        m_textbox->focus(x);
    }
}
