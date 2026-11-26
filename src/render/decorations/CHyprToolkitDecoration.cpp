#include "CHyprToolkitDecoration.hpp"
#include "DecorationPositioner.hpp"
#include "ToolkitDecorationProvider.hpp"

#include <hyprtoolkit/core/EmbeddedBackend.hpp>
#include <hyprtoolkit/element/Element.hpp>
#include <hyprtoolkit/types/PointerShape.hpp>
#include <hyprtoolkit/window/EmbeddedSurface.hpp>
#include <hyprutils/memory/Casts.hpp>

#include <linux/input-event-codes.h>

#include <format>

#include "../../debug/log/Logger.hpp"
#include "../../desktop/rule/windowRule/WindowRuleApplicator.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"
#include "../../devices/IPointer.hpp"
#include "../../event/EventBus.hpp"
#include "../../hyprtoolkit/Host.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../layout/supplementary/DragController.hpp"
#include "../../output/Monitor.hpp"
#include "../../pointer/cursor/CursorShapeOverrideController.hpp"
#include "../Framebuffer.hpp"
#include "../OpenGL.hpp"
#include "../Renderer.hpp"
#include "../WindowRenderPresentation.hpp"
#include "../pass/TexPassElement.hpp"

using namespace Render;

static Hyprtoolkit::Input::eMouseButton toolkitButton(uint32_t button) {
    switch (button) {
        case BTN_LEFT: return Hyprtoolkit::Input::MOUSE_BUTTON_LEFT;
        case BTN_RIGHT: return Hyprtoolkit::Input::MOUSE_BUTTON_RIGHT;
        case BTN_MIDDLE: return Hyprtoolkit::Input::MOUSE_BUTTON_MIDDLE;
        default: return Hyprtoolkit::Input::MOUSE_BUTTON_UNKNOWN;
    }
}

// nullptr for the default cursor
static const char* cursorName(Hyprtoolkit::ePointerShape shape) {
    switch (shape) {
        case Hyprtoolkit::HT_POINTER_POINTER: return "pointer";
        case Hyprtoolkit::HT_POINTER_TEXT: return "text";
        case Hyprtoolkit::HT_POINTER_RESIZE_NS: return "ns-resize";
        case Hyprtoolkit::HT_POINTER_RESIZE_EW: return "ew-resize";
        case Hyprtoolkit::HT_POINTER_RESIZE_NESW: return "nesw-resize";
        case Hyprtoolkit::HT_POINTER_RESIZE_NWSE: return "nwse-resize";
        default: return nullptr;
    }
}

static bool toolkitAvailable() {
    return Toolkit::host() && Toolkit::host()->backend();
}

CHyprToolkitDecoration::CHyprToolkitDecoration(PHLWINDOW window, SP<IToolkitDecorationProvider> provider) : IHyprWindowDecoration(window), m_provider(std::move(provider)) {
    if (toolkitAvailable())
        m_surface = Toolkit::host()->backend()->createSurface();

    if (!m_surface) {
        LOG(Log::ERR, "[hyprtoolkit] couldn't create a surface for decoration \"{}\"", m_provider->name());
        return;
    }

    m_listeners.frameRequested = m_surface->m_events.frameRequested.listen([this] { onFrameRequested(); });
    m_listeners.damaged        = m_surface->m_events.damaged.listen([this](const CRegion& damage) { onDamage(damage); });
    m_listeners.cursorChanged  = m_surface->m_events.cursorChanged.listen([this](Hyprtoolkit::ePointerShape shape) { onCursorChanged(shape); });
}

CHyprToolkitDecoration::~CHyprToolkitDecoration() {
    m_dragPending = false;
    pointerLeave();
    releaseClient();
}

SDecorationPositioningInfo CHyprToolkitDecoration::getPositioningInfo() {
    const auto&                SPEC = m_provider->spec();
    const auto                 SIZE = m_visible ? SPEC.size : 0;

    SDecorationPositioningInfo info;
    info.policy   = DECORATION_POSITION_STICKY;
    info.edges    = SPEC.edge;
    info.priority = SPEC.priority;
    info.reserved = SPEC.reserved;

    switch (SPEC.edge) {
        case DECORATION_EDGE_TOP: info.desiredExtents = {{0, SIZE}, {0, 0}}; break;
        case DECORATION_EDGE_BOTTOM: info.desiredExtents = {{0, 0}, {0, SIZE}}; break;
        case DECORATION_EDGE_LEFT: info.desiredExtents = {{SIZE, 0}, {0, 0}}; break;
        case DECORATION_EDGE_RIGHT: info.desiredExtents = {{0, 0}, {SIZE, 0}}; break;
    }

    return info;
}

void CHyprToolkitDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    m_assignedBox = reply.assignedGeometry;

    // the size is picked up before the next render
    damageEntire();
}

void CHyprToolkitDecoration::draw(CRenderContext& ctx, PHLMONITOR pMonitor, float const& a, const SWindowRenderPresentation& presentation) {
    if (!m_visible || !m_rendered || !m_framebuffer)
        return;

    CBox box = renderBox(presentation);
    box.translate(-pMonitor->m_position).scale(pMonitor->m_scale).round();

    if (box.empty())
        return;

    CTexPassElement::SRenderData data;
    data.tex                   = m_framebuffer->getTexture();
    data.box                   = box;
    data.a                     = a;
    data.workspacePresentation = presentation;
    g_pHyprRenderer->addPassElement(ctx, makeUnique<CTexPassElement>(std::move(data)));
}

eDecorationType CHyprToolkitDecoration::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CHyprToolkitDecoration::updateWindow(PHLWINDOW pWindow) {
    const bool VISIBLE = pWindow->m_ruleApplicator->decorate().valueOrDefault();

    if (VISIBLE != m_visible) {
        m_visible = VISIBLE;
        g_pDecorationPositioner->repositionDeco(this);
    }

    damageEntire();
}

void CHyprToolkitDecoration::damageEntire() {
    const auto WINDOW = window();
    if (!WINDOW)
        return;

    g_pHyprRenderer->damageBox(renderBox(WINDOW->presentation().renderPresentation()));
}

bool CHyprToolkitDecoration::onInputOnDeco(const eInputType type, const Vector2D& coords, std::any data) {
    if (!m_surface)
        return false;

    // input may end up in callbacks which remove this decoration
    const auto SELF = self();

    switch (type) {
        case INPUT_TYPE_BUTTON: return onButton(coords, data);
        case INPUT_TYPE_AXIS: return onAxis(coords, data);
        default: return false;
    }
}

eDecorationLayer CHyprToolkitDecoration::getDecorationLayer() {
    return m_provider->spec().layer;
}

uint64_t CHyprToolkitDecoration::getDecorationFlags() {
    return m_provider->spec().input ? DECORATION_ALLOWS_MOUSE_INPUT : 0;
}

std::string CHyprToolkitDecoration::getDisplayName() {
    return std::format("Toolkit: {}", m_provider->name());
}

void CHyprToolkitDecoration::createClient() {
    if (!m_surface || m_client)
        return;

    m_client = m_provider->createClient(dynamicPointerCast<CHyprToolkitDecoration>(self()));
}

void CHyprToolkitDecoration::releaseClient() {
    m_client.reset();

    // the widgets can't outlive the backend
    if (m_surface && toolkitAvailable())
        m_surface->rootElement()->clearChildren();
}

void CHyprToolkitDecoration::renderIfNeeded() {
    if (!m_surface || !updateSize() || !m_needsRender)
        return;

    if (!m_framebuffer)
        m_framebuffer = g_pHyprRenderer->createFB("toolkit decoration");

    const bool REALLOCATED = !m_framebuffer->isAllocated() || m_framebuffer->m_size != m_pixelSize;
    if (!m_framebuffer->alloc(m_pixelSize.x, m_pixelSize.y))
        return;

    // before rendering: animations request their next frame while rendering
    m_needsRender = false;

    GL::CFramebufferBindingGuard guard(g_pHyprRenderer->glBackend());
    m_framebuffer->bind();

    try {
        m_surface->render(REALLOCATED ? 0 : 1);
        m_rendered = true;
    } catch (const std::exception& e) { LOG(Log::ERR, "[hyprtoolkit] decoration \"{}\" failed to render: {}", m_provider->name(), e.what()); }
}

void CHyprToolkitDecoration::pointerMotion(const Vector2D& coords) {
    if (!m_surface)
        return;

    const auto SELF  = self();
    const auto LOCAL = toLocal(coords);

    if (!m_hovered) {
        m_hovered = true;
        m_surface->pointerEnter(LOCAL);
    }

    m_surface->pointerMotion(LOCAL);
    startPendingDrag();

    if (const auto WINDOW = window())
        Event::bus()->m_events.decoration.pointerMove.emit(WINDOW, m_provider->name(), LOCAL);
}

void CHyprToolkitDecoration::pointerLeave() {
    if (!m_hovered)
        return;

    // a quick first motion may leave the decoration right away
    startPendingDrag();

    m_hovered     = false;
    m_leftPressed = false;

    if (m_cursorSet) {
        Pointer::Cursor::overrideController->unsetOverride(Pointer::Cursor::CURSOR_OVERRIDE_DECORATION);
        m_cursorSet = false;
    }

    if (m_surface)
        m_surface->pointerLeave();
}

void CHyprToolkitDecoration::beginDrag() {
    m_dragPending = m_leftPressed;
}

void CHyprToolkitDecoration::startPendingDrag() {
    if (!m_dragPending)
        return;

    // only on motion, a plain click shouldn't pick up the window. CKeybindManager::onMouseEvent ends it on release.
    m_dragPending = false;

    if (const auto WINDOW = window(); WINDOW && WINDOW->layoutTarget() && !g_layoutManager->dragController()->target())
        g_layoutManager->beginDragTarget(WINDOW->layoutTarget(), MBIND_MOVE);
}

PHLWINDOW CHyprToolkitDecoration::window() const {
    return owningWindow();
}

SP<IToolkitDecorationProvider> CHyprToolkitDecoration::provider() const {
    return m_provider;
}

SP<Hyprtoolkit::IEmbeddedSurface> CHyprToolkitDecoration::surface() const {
    return m_surface;
}

CBox CHyprToolkitDecoration::renderBox(const SWindowRenderPresentation& presentation) const {
    const auto WINDOW = window();
    if (!WINDOW)
        return {};

    CBox box = m_assignedBox;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(m_provider->spec().edge, WINDOW));
    box.translate(presentation.workspaceOffset + presentation.floatingOffset);
    return box;
}

Vector2D CHyprToolkitDecoration::toLocal(const Vector2D& coords) {
    return coords - g_pDecorationPositioner->getWindowDecorationBox(this).pos();
}

bool CHyprToolkitDecoration::updateSize() {
    const auto WINDOW = window();
    if (!WINDOW || !WINDOW->m_monitor)
        return false;

    // render at the scale of the window's monitor, a window spanning more of them would thrash otherwise
    const float SCALE   = WINDOW->m_monitor->m_scale;
    const auto  LOGICAL = m_assignedBox.size();
    const auto  PIXELS  = (LOGICAL * SCALE).round();

    if (PIXELS.x < 1 || PIXELS.y < 1)
        return false;

    if (PIXELS == m_pixelSize && SCALE == m_scale)
        return true;

    m_pixelSize = PIXELS;
    m_scale     = SCALE;
    m_surface->resize(LOGICAL, PIXELS, SCALE);
    return true;
}

void CHyprToolkitDecoration::onFrameRequested() {
    m_needsRender = true;

    if (const auto WINDOW = window(); WINDOW && WINDOW->m_monitor)
        WINDOW->m_monitor->scheduleFrame();
}

void CHyprToolkitDecoration::onDamage(const CRegion& damage) {
    const auto WINDOW = window();
    if (!WINDOW)
        return;

    CRegion global = damage.copy();
    global.translate(renderBox(WINDOW->presentation().renderPresentation()).pos());
    g_pHyprRenderer->damageRegion(global);
}

void CHyprToolkitDecoration::onCursorChanged(Hyprtoolkit::ePointerShape shape) {
    if (!m_hovered)
        return;

    if (const auto NAME = cursorName(shape); NAME) {
        Pointer::Cursor::overrideController->setOverride(NAME, Pointer::Cursor::CURSOR_OVERRIDE_DECORATION);
        m_cursorSet = true;
        return;
    }

    if (m_cursorSet) {
        Pointer::Cursor::overrideController->unsetOverride(Pointer::Cursor::CURSOR_OVERRIDE_DECORATION);
        m_cursorSet = false;
    }
}

bool CHyprToolkitDecoration::onButton(const Vector2D& coords, const std::any& data) {
    const auto& EVENT   = std::any_cast<const IPointer::SButtonEvent&>(data);
    const bool  PRESSED = EVENT.state == WL_POINTER_BUTTON_STATE_PRESSED;

    if (!m_hovered)
        pointerMotion(coords);

    // before the callbacks below, they may begin a drag
    if (EVENT.button == BTN_LEFT) {
        m_leftPressed = PRESSED;
        m_dragPending = false;
    }

    if (const auto WINDOW = window())
        Event::bus()->m_events.decoration.pointerButton.emit(WINDOW, m_provider->name(), EVENT.button, PRESSED, toLocal(coords));

    if (const auto BUTTON = toolkitButton(EVENT.button); BUTTON != Hyprtoolkit::Input::MOUSE_BUTTON_UNKNOWN)
        m_surface->pointerButton(BUTTON, PRESSED);

    return true;
}

bool CHyprToolkitDecoration::onAxis(const Vector2D& coords, const std::any& data) {
    const auto& EVENT = std::any_cast<const IPointer::SAxisEvent&>(data);

    if (!m_hovered)
        pointerMotion(coords);

    m_surface->pointerAxis(EVENT.axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL ? Hyprtoolkit::Input::AXIS_AXIS_HORIZONTAL : Hyprtoolkit::Input::AXIS_AXIS_VERTICAL, EVENT.delta);
    return true;
}
