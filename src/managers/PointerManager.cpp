#include "PointerManager.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/PointerGestures.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/core/Compositor.hpp"
#include "SeatManager.hpp"
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/interface.h>
#include <wlr/render/wlr_renderer.h>

// TODO: make nicer
// this will come with the eventual rewrite of wlr_drm, etc...
static bool wlr_drm_format_intersect(wlr_drm_format* dst, const wlr_drm_format* a, const wlr_drm_format* b) {
    ASSERT(a->format == b->format);

    size_t    capacity  = a->len < b->len ? a->len : b->len;
    uint64_t* modifiers = (uint64_t*)malloc(sizeof(*modifiers) * capacity);
    if (!modifiers)
        return false;

    struct wlr_drm_format fmt = {
        .format    = a->format,
        .len       = 0,
        .capacity  = capacity,
        .modifiers = modifiers,
    };

    for (size_t i = 0; i < a->len; i++) {
        for (size_t j = 0; j < b->len; j++) {
            if (a->modifiers[i] == b->modifiers[j]) {
                ASSERT(fmt.len < fmt.capacity);
                fmt.modifiers[fmt.len++] = a->modifiers[i];
                break;
            }
        }
    }

    wlr_drm_format_finish(dst);
    *dst = fmt;
    return true;
}

static bool wlr_drm_format_copy(wlr_drm_format* dst, const wlr_drm_format* src) {
    ASSERT(src->len <= src->capacity);

    uint64_t* modifiers = (uint64_t*)malloc(sizeof(*modifiers) * src->len);
    if (!modifiers)
        return false;

    memcpy(modifiers, src->modifiers, sizeof(*modifiers) * src->len);

    wlr_drm_format_finish(dst);
    dst->capacity  = src->len;
    dst->len       = src->len;
    dst->format    = src->format;
    dst->modifiers = modifiers;
    return true;
}

static const wlr_drm_format_set* wlr_renderer_get_render_formats(wlr_renderer* r) {
    if (!r->impl->get_render_formats)
        return nullptr;

    return r->impl->get_render_formats(r);
}

static bool output_pick_format(wlr_output* output, const wlr_drm_format_set* display_formats, wlr_drm_format* format, uint32_t fmt) {

    const wlr_drm_format_set* render_formats = wlr_renderer_get_render_formats(g_pCompositor->m_sWLRRenderer);
    if (render_formats == NULL) {
        wlr_log(WLR_ERROR, "Failed to get render formats");
        return false;
    }

    const wlr_drm_format* render_format = wlr_drm_format_set_get(render_formats, fmt);
    if (render_format == NULL) {
        wlr_log(WLR_DEBUG, "Renderer doesn't support format 0x%" PRIX32, fmt);
        return false;
    }

    if (display_formats != NULL) {
        const wlr_drm_format* display_format = wlr_drm_format_set_get(display_formats, fmt);
        if (display_format == NULL) {
            wlr_log(WLR_DEBUG, "Output doesn't support format 0x%" PRIX32, fmt);
            return false;
        }
        if (!wlr_drm_format_intersect(format, display_format, render_format)) {
            wlr_log(WLR_DEBUG,
                    "Failed to intersect display and render "
                    "modifiers for format 0x%" PRIX32 " on output %s",
                    fmt, output->name);
            return false;
        }
    } else {
        // The output can display any format
        if (!wlr_drm_format_copy(format, render_format))
            return false;
    }

    if (format->len == 0) {
        wlr_drm_format_finish(format);
        wlr_log(WLR_DEBUG, "Failed to pick output format");
        return false;
    }

    return true;
}

static bool output_pick_cursor_format(struct wlr_output* output, struct wlr_drm_format* format) {
    struct wlr_allocator* allocator = output->allocator;
    ASSERT(allocator != NULL);

    const struct wlr_drm_format_set* display_formats = NULL;
    if (output->impl->get_cursor_formats) {
        display_formats = output->impl->get_cursor_formats(output, allocator->buffer_caps);
        if (display_formats == NULL) {
            wlr_log(WLR_DEBUG, "Failed to get cursor display formats");
            return false;
        }
    }

    // Note: taken from https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4596/diffs#diff-content-e3ea164da86650995728d70bd118f6aa8c386797
    // If this fails to find a shared modifier try to use a linear
    // modifier. This avoids a scenario where the hardware cannot render to
    // linear textures but only linear textures are supported for cursors,
    // as is the case with Nvidia and VmWare GPUs
    if (!output_pick_format(output, display_formats, format, DRM_FORMAT_ARGB8888)) {
        // Clear the format as output_pick_format doesn't zero it
        memset(format, 0, sizeof(*format));
        return output_pick_format(output, NULL, format, DRM_FORMAT_ARGB8888);
    }
    return true;
}

CPointerManager::CPointerManager() {
    hooks.monitorAdded = g_pHookSystem->hookDynamic("newMonitor", [this](void* self, SCallbackInfo& info, std::any data) {
        auto PMONITOR = std::any_cast<SP<CMonitor>>(data);

        onMonitorLayoutChange();

        PMONITOR->events.modeChanged.registerStaticListener([this](void* owner, std::any data) { onMonitorLayoutChange(); }, nullptr);
        PMONITOR->events.disconnect.registerStaticListener([this](void* owner, std::any data) { onMonitorLayoutChange(); }, nullptr);
        PMONITOR->events.destroy.registerStaticListener(
            [this](void* owner, std::any data) {
                if (g_pCompositor && !g_pCompositor->m_bIsShuttingDown)
                    std::erase_if(monitorStates, [](const auto& other) { return other->monitor.expired(); });
            },
            nullptr);
    });
}

void CPointerManager::lockSoftwareForMonitor(SP<CMonitor> mon) {
    auto state = stateFor(mon);
    state->softwareLocks++;

    if (state->softwareLocks == 1)
        updateCursorBackend();
}

void CPointerManager::unlockSoftwareForMonitor(SP<CMonitor> mon) {
    auto state = stateFor(mon);
    state->softwareLocks--;
    if (state->softwareLocks < 0)
        state->softwareLocks = 0;

    if (state->softwareLocks == 0)
        updateCursorBackend();
}

Vector2D CPointerManager::position() {
    return pointerPos;
}

bool CPointerManager::hasCursor() {
    return currentCursorImage.pBuffer || currentCursorImage.surface;
}

SP<CPointerManager::SMonitorPointerState> CPointerManager::stateFor(SP<CMonitor> mon) {
    auto it = std::find_if(monitorStates.begin(), monitorStates.end(), [mon](const auto& other) { return other->monitor == mon; });
    if (it == monitorStates.end())
        return monitorStates.emplace_back(makeShared<CPointerManager::SMonitorPointerState>(mon));
    return *it;
}

void CPointerManager::setCursorBuffer(wlr_buffer* buf, const Vector2D& hotspot, const float& scale) {
    damageIfSoftware();
    if (buf == currentCursorImage.pBuffer) {
        if (hotspot != currentCursorImage.hotspot || scale != currentCursorImage.scale) {
            currentCursorImage.hotspot = hotspot;
            currentCursorImage.scale   = scale;
            updateCursorBackend();
            damageIfSoftware();
        }

        return;
    }

    resetCursorImage(false);

    if (buf) {
        currentCursorImage.size    = {buf->width, buf->height};
        currentCursorImage.pBuffer = wlr_buffer_lock(buf);

        currentCursorImage.hyprListener_destroyBuffer.initCallback(
            &buf->events.destroy, [this](void* owner, void* data) { resetCursorImage(); }, this, "CPointerManager");
    }

    currentCursorImage.hotspot = hotspot;
    currentCursorImage.scale   = scale;

    updateCursorBackend();
    damageIfSoftware();
}

void CPointerManager::setCursorSurface(SP<CWLSurface> surf, const Vector2D& hotspot) {
    damageIfSoftware();

    if (surf == currentCursorImage.surface) {
        if (hotspot != currentCursorImage.hotspot || (surf && surf->resource() ? surf->resource()->current.scale : 1.F) != currentCursorImage.scale) {
            currentCursorImage.hotspot = hotspot;
            currentCursorImage.scale   = surf && surf->resource() ? surf->resource()->current.scale : 1.F;
            updateCursorBackend();
            damageIfSoftware();
        }

        return;
    }

    resetCursorImage(false);

    if (surf) {
        currentCursorImage.surface = surf;
        currentCursorImage.scale   = surf->resource()->current.scale;

        currentCursorImage.destroySurface = surf->events.destroy.registerListener([this](std::any data) { resetCursorImage(); });
        currentCursorImage.commitSurface  = surf->resource()->events.commit.registerListener([this](std::any data) {
            damageIfSoftware();
            currentCursorImage.size  = currentCursorImage.surface->resource()->current.buffer ? currentCursorImage.surface->resource()->current.buffer->size : Vector2D{};
            currentCursorImage.scale = currentCursorImage.surface ? currentCursorImage.surface->resource()->current.scale : 1.F;
            recheckEnteredOutputs();
            updateCursorBackend();
            damageIfSoftware();
        });

        if (surf->resource()->current.buffer) {
            currentCursorImage.size = surf->resource()->current.buffer->size;
            timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            surf->resource()->frame(&now);
        }
    }

    currentCursorImage.hotspot = hotspot;

    recheckEnteredOutputs();
    updateCursorBackend();
    damageIfSoftware();
}

void CPointerManager::recheckEnteredOutputs() {
    if (!hasCursor())
        return;

    auto box = getCursorBoxGlobal();

    for (auto& s : monitorStates) {
        if (s->monitor.expired() || s->monitor->isMirror() || !s->monitor->m_bEnabled)
            continue;

        const bool overlaps = box.overlaps(s->monitor->logicalBox());

        if (!s->entered && overlaps) {
            s->entered = true;

            if (!currentCursorImage.surface)
                continue;

            currentCursorImage.surface->resource()->enter(s->monitor.lock());
            PROTO::fractional->sendScale(currentCursorImage.surface->resource(), s->monitor->scale);
            g_pCompositor->setPreferredScaleForSurface(currentCursorImage.surface->resource(), s->monitor->scale);
        } else if (s->entered && !overlaps) {
            s->entered = false;

            // if we are using hw cursors, prevent
            // the cursor from being stuck at the last point.
            // if we are leaving it, move it to narnia.
            if (!s->hardwareFailed && s->monitor->output->impl->move_cursor)
                s->monitor->output->impl->move_cursor(s->monitor->output, -1337, -420);

            if (!currentCursorImage.surface)
                continue;

            currentCursorImage.surface->resource()->leave(s->monitor.lock());
        }
    }
}

void CPointerManager::resetCursorImage(bool apply) {
    damageIfSoftware();

    if (currentCursorImage.surface) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            currentCursorImage.surface->resource()->leave(m);
        }

        currentCursorImage.destroySurface.reset();
        currentCursorImage.commitSurface.reset();
        currentCursorImage.surface.reset();
    } else if (currentCursorImage.pBuffer) {
        wlr_buffer_unlock(currentCursorImage.pBuffer);
        currentCursorImage.hyprListener_destroyBuffer.removeCallback();
        currentCursorImage.pBuffer = nullptr;
    }

    if (currentCursorImage.pBufferTexture) {
        wlr_texture_destroy(currentCursorImage.pBufferTexture);
        currentCursorImage.pBufferTexture = nullptr;
    }

    currentCursorImage.scale   = 1.F;
    currentCursorImage.hotspot = {0, 0};

    for (auto& s : monitorStates) {
        if (s->monitor.expired() || s->monitor->isMirror() || !s->monitor->m_bEnabled)
            continue;

        s->entered = false;
    }

    if (!apply)
        return;

    for (auto& ms : monitorStates) {
        if (!ms->monitor || !ms->monitor->m_bEnabled || !ms->monitor->dpmsStatus) {
            Debug::log(TRACE, "Not updating hw cursors: disabled / dpms off display");
            continue;
        }

        if (ms->cursorFrontBuffer) {
            if (ms->monitor->output->impl->set_cursor)
                ms->monitor->output->impl->set_cursor(ms->monitor->output, nullptr, 0, 0);
            wlr_buffer_unlock(ms->cursorFrontBuffer);
            ms->cursorFrontBuffer = nullptr;
        }
    }
}

void CPointerManager::updateCursorBackend() {
    static auto PNOHW = CConfigValue<Hyprlang::INT>("cursor:no_hardware_cursors");

    for (auto& m : g_pCompositor->m_vMonitors) {
        auto state = stateFor(m);

        if (!m->m_bEnabled || !m->dpmsStatus) {
            Debug::log(TRACE, "Not updating hw cursors: disabled / dpms off display");
            continue;
        }

        if (state->softwareLocks > 0 || *PNOHW || !attemptHardwareCursor(state)) {
            Debug::log(TRACE, "Output {} rejected hardware cursors, falling back to sw", m->szName);
            state->box            = getCursorBoxLogicalForMonitor(state->monitor.lock());
            state->hardwareFailed = true;

            if (state->hwApplied)
                setHWCursorBuffer(state, nullptr);

            state->hwApplied = false;
            continue;
        }

        state->hardwareFailed = false;
    }
}

void CPointerManager::onCursorMoved() {
    if (!hasCursor())
        return;

    for (auto& m : g_pCompositor->m_vMonitors) {
        auto state = stateFor(m);

        state->box = getCursorBoxLogicalForMonitor(state->monitor.lock());

        if (state->hardwareFailed || !state->entered)
            continue;

        const auto CURSORPOS = getCursorPosForMonitor(m);
        m->output->impl->move_cursor(m->output, CURSORPOS.x, CURSORPOS.y);
    }
}

bool CPointerManager::attemptHardwareCursor(SP<CPointerManager::SMonitorPointerState> state) {
    auto output = state->monitor->output;

    if (!output->impl->set_cursor)
        return false;

    const auto CURSORPOS = getCursorPosForMonitor(state->monitor.lock());
    state->monitor->output->impl->move_cursor(state->monitor->output, CURSORPOS.x, CURSORPOS.y);

    auto texture = getCurrentCursorTexture();

    if (!texture) {
        Debug::log(TRACE, "[pointer] no texture for hw cursor -> hiding");
        setHWCursorBuffer(state, nullptr);
        return true;
    }

    auto buffer = renderHWCursorBuffer(state, texture);

    if (!buffer) {
        Debug::log(TRACE, "[pointer] hw cursor failed rendering");
        setHWCursorBuffer(state, nullptr);
        return false;
    }

    bool success = setHWCursorBuffer(state, buffer);

    if (!success) {
        Debug::log(TRACE, "[pointer] hw cursor failed applying, hiding");
        setHWCursorBuffer(state, nullptr);
        return false;
    } else
        state->hwApplied = true;

    return success;
}

bool CPointerManager::setHWCursorBuffer(SP<SMonitorPointerState> state, wlr_buffer* buf) {
    if (!state->monitor->output->impl->set_cursor)
        return false;

    const auto HOTSPOT = transformedHotspot(state->monitor.lock());

    Debug::log(TRACE, "[pointer] hw transformed hotspot for {}: {}", state->monitor->szName, HOTSPOT);

    if (!state->monitor->output->impl->set_cursor(state->monitor->output, buf, HOTSPOT.x, HOTSPOT.y))
        return false;

    wlr_buffer_unlock(state->cursorFrontBuffer);
    state->cursorFrontBuffer = buf;

    g_pCompositor->scheduleFrameForMonitor(state->monitor.get());

    if (buf)
        wlr_buffer_lock(buf);

    return true;
}

wlr_buffer* CPointerManager::renderHWCursorBuffer(SP<CPointerManager::SMonitorPointerState> state, SP<CTexture> texture) {
    auto output = state->monitor->output;

    int  w = currentCursorImage.size.x, h = currentCursorImage.size.y;
    if (output->impl->get_cursor_size) {
        output->impl->get_cursor_size(output, &w, &h);

        if (w < currentCursorImage.size.x || h < currentCursorImage.size.y) {
            Debug::log(TRACE, "hardware cursor too big! {} > {}x{}", currentCursorImage.size, w, h);
            return nullptr;
        }
    }

    if (w <= 0 || h <= 0) {
        Debug::log(TRACE, "hw cursor for output {} failed the size checks ({}x{} is invalid)", state->monitor->szName, w, h);
        return nullptr;
    }

    if (!output->cursor_swapchain || Vector2D{w, h} != Vector2D{output->cursor_swapchain->width, output->cursor_swapchain->height}) {
        wlr_drm_format fmt = {0};
        if (!output_pick_cursor_format(output, &fmt)) {
            Debug::log(TRACE, "Failed to pick cursor format");
            return nullptr;
        }

        wlr_swapchain_destroy(output->cursor_swapchain);
        output->cursor_swapchain = wlr_swapchain_create(output->allocator, w, h, &fmt);
        wlr_drm_format_finish(&fmt);

        if (!output->cursor_swapchain) {
            Debug::log(TRACE, "Failed to create cursor swapchain");
            return nullptr;
        }
    }

    wlr_buffer* buf = wlr_swapchain_acquire(output->cursor_swapchain, nullptr);
    if (!buf) {
        Debug::log(TRACE, "Failed to acquire a buffer from the cursor swapchain");
        return nullptr;
    }

    CRegion damage = {0, 0, INT16_MAX, INT16_MAX};

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_RenderData.pMonitor = state->monitor.get(); // has to be set cuz allocs

    const auto RBO = g_pHyprRenderer->getOrCreateRenderbuffer(buf, DRM_FORMAT_ARGB8888);
    RBO->bind();

    g_pHyprOpenGL->beginSimple(state->monitor.get(), damage, RBO);
    g_pHyprOpenGL->clear(CColor{0.F, 0.F, 0.F, 0.F});

    CBox xbox = {{}, Vector2D{currentCursorImage.size / currentCursorImage.scale * state->monitor->scale}.round()};
    Debug::log(TRACE, "[pointer] monitor: {}, size: {}, hw buf: {}, scale: {:.2f}, monscale: {:.2f}, xbox: {}", state->monitor->szName, currentCursorImage.size, Vector2D{w, h},
               currentCursorImage.scale, state->monitor->scale, xbox.size());

    g_pHyprOpenGL->renderTexture(texture, &xbox, 1.F);

    g_pHyprOpenGL->end();
    glFlush();
    g_pHyprOpenGL->m_RenderData.pMonitor = nullptr;

    g_pHyprRenderer->onRenderbufferDestroy(RBO);

    wlr_buffer_unlock(buf);

    return buf;
}

void CPointerManager::renderSoftwareCursorsFor(SP<CMonitor> pMonitor, timespec* now, CRegion& damage, std::optional<Vector2D> overridePos) {
    if (!hasCursor())
        return;

    auto state = stateFor(pMonitor);

    if ((!state->hardwareFailed && state->softwareLocks == 0)) {
        if (currentCursorImage.surface)
            currentCursorImage.surface->resource()->frame(now);
        return;
    }

    auto box = state->box.copy();
    if (overridePos.has_value()) {
        box.x = overridePos->x;
        box.y = overridePos->y;
    }

    if (box.intersection(CBox{{}, {pMonitor->vecSize}}).empty())
        return;

    auto texture = getCurrentCursorTexture();
    if (!texture)
        return;

    box.scale(pMonitor->scale);

    g_pHyprOpenGL->renderTextureWithDamage(texture, &box, &damage, 1.F);

    if (currentCursorImage.surface)
        currentCursorImage.surface->resource()->frame(now);
}

Vector2D CPointerManager::getCursorPosForMonitor(SP<CMonitor> pMonitor) {
    return CBox{pointerPos - pMonitor->vecPosition, {0, 0}}
               //.transform(pMonitor->transform, pMonitor->vecTransformedSize.x / pMonitor->scale, pMonitor->vecTransformedSize.y / pMonitor->scale)
               .pos() *
        pMonitor->scale;
}

Vector2D CPointerManager::transformedHotspot(SP<CMonitor> pMonitor) {
    if (!pMonitor->output->cursor_swapchain)
        return {}; // doesn't matter, we have no hw cursor, and this is only for hw cursors

    return CBox{currentCursorImage.hotspot * pMonitor->scale, {0, 0}}
        .transform(wlr_output_transform_invert(pMonitor->transform), pMonitor->output->cursor_swapchain->width, pMonitor->output->cursor_swapchain->height)
        .pos();
}

CBox CPointerManager::getCursorBoxLogicalForMonitor(SP<CMonitor> pMonitor) {
    return getCursorBoxGlobal().translate(-pMonitor->vecPosition);
}

CBox CPointerManager::getCursorBoxGlobal() {
    return CBox{pointerPos, currentCursorImage.size / currentCursorImage.scale}.translate(-currentCursorImage.hotspot);
}

Vector2D CPointerManager::closestValid(const Vector2D& pos) {
    static auto PADDING = CConfigValue<Hyprlang::INT>("cursor:hotspot_padding");

    auto        CURSOR_PADDING = std::clamp((int)*PADDING, 1, 100); // 1px
    CBox        hotBox         = {{pos.x - CURSOR_PADDING, pos.y - CURSOR_PADDING}, {2 * CURSOR_PADDING, 2 * CURSOR_PADDING}};

    //
    static auto INSIDE_LAYOUT = [this](const CBox& box) -> bool {
        for (auto& b : currentMonitorLayout.monitorBoxes) {
            if (box.inside(b))
                return true;
        }
        return false;
    };

    static auto INSIDE_LAYOUT_COORD = [this](const Vector2D& vec) -> bool {
        for (auto& b : currentMonitorLayout.monitorBoxes) {
            if (b.containsPoint(vec))
                return true;
        }
        return false;
    };

    static auto NEAREST_LAYOUT = [this](const Vector2D& vec) -> Vector2D {
        Vector2D leader;
        float    distanceSq = __FLT_MAX__;

        for (auto& b : currentMonitorLayout.monitorBoxes) {
            auto p      = b.closestPoint(vec);
            auto distSq = p.distanceSq(vec);

            if (distSq < distanceSq) {
                leader     = p;
                distanceSq = distSq;
            }
        }

        if (distanceSq > 1337.69420e+20F)
            return {0, 0}; // ???

        return leader;
    };

    if (INSIDE_LAYOUT(hotBox))
        return pos;

    Vector2D leader = NEAREST_LAYOUT(pos);

    hotBox.x = leader.x - CURSOR_PADDING;
    hotBox.y = leader.y - CURSOR_PADDING;

    // push the hotbox around so that it fits in the layout

    if (!INSIDE_LAYOUT_COORD(hotBox.middle() + Vector2D{CURSOR_PADDING, CURSOR_PADDING})) {
        auto delta = NEAREST_LAYOUT(hotBox.middle() + Vector2D{CURSOR_PADDING, CURSOR_PADDING}) - (hotBox.middle() + Vector2D{CURSOR_PADDING, CURSOR_PADDING});
        hotBox.translate(delta);
    }

    if (!INSIDE_LAYOUT_COORD(hotBox.middle() - Vector2D{CURSOR_PADDING, CURSOR_PADDING})) {
        auto delta = NEAREST_LAYOUT(hotBox.middle() - Vector2D{CURSOR_PADDING, CURSOR_PADDING}) - (hotBox.middle() - Vector2D{CURSOR_PADDING, CURSOR_PADDING});
        hotBox.translate(delta);
    }

    if (!INSIDE_LAYOUT_COORD(hotBox.middle() + Vector2D{CURSOR_PADDING, -CURSOR_PADDING})) {
        auto delta = NEAREST_LAYOUT(hotBox.middle() + Vector2D{CURSOR_PADDING, -CURSOR_PADDING}) - (hotBox.middle() + Vector2D{CURSOR_PADDING, -CURSOR_PADDING});
        hotBox.translate(delta);
    }

    if (!INSIDE_LAYOUT_COORD(hotBox.middle() + Vector2D{-CURSOR_PADDING, CURSOR_PADDING})) {
        auto delta = NEAREST_LAYOUT(hotBox.middle() + Vector2D{-CURSOR_PADDING, CURSOR_PADDING}) - (hotBox.middle() + Vector2D{-CURSOR_PADDING, CURSOR_PADDING});
        hotBox.translate(delta);
    }

    return hotBox.middle();
}

void CPointerManager::damageIfSoftware() {
    auto        b = getCursorBoxGlobal();

    static auto PNOHW = CConfigValue<Hyprlang::INT>("cursor:no_hardware_cursors");

    for (auto& mw : monitorStates) {
        if (mw->monitor.expired())
            continue;

        if ((mw->softwareLocks > 0 || mw->hardwareFailed || *PNOHW) && b.overlaps({mw->monitor->vecPosition, mw->monitor->vecSize})) {
            g_pHyprRenderer->damageBox(&b);
            break;
        }
    }
}

void CPointerManager::warpTo(const Vector2D& logical) {
    damageIfSoftware();

    pointerPos = closestValid(logical);
    recheckEnteredOutputs();
    onCursorMoved();

    damageIfSoftware();
}

void CPointerManager::move(const Vector2D& deltaLogical) {
    const auto oldPos = pointerPos;
    auto       newPos = oldPos + Vector2D{std::isnan(deltaLogical.x) ? 0.0 : deltaLogical.x, std::isnan(deltaLogical.y) ? 0.0 : deltaLogical.y};

    warpTo(newPos);
}

void CPointerManager::warpAbsolute(Vector2D abs, SP<IHID> dev) {

    SP<CMonitor> currentMonitor = g_pCompositor->m_pLastMonitor.lock();
    if (!currentMonitor)
        return;

    if (!std::isnan(abs.x))
        abs.x = std::clamp(abs.x, 0.0, 1.0);
    if (!std::isnan(abs.y))
        abs.y = std::clamp(abs.y, 0.0, 1.0);

    // in logical global
    CBox mappedArea = currentMonitor->logicalBox();

    switch (dev->getType()) {
        case HID_TYPE_TABLET: {
            CTablet* TAB = reinterpret_cast<CTablet*>(dev.get());
            if (!TAB->boundOutput.empty()) {
                if (const auto PMONITOR = g_pCompositor->getMonitorFromString(TAB->boundOutput); PMONITOR) {
                    currentMonitor = PMONITOR->self.lock();
                    mappedArea     = currentMonitor->logicalBox();
                }
            }

            mappedArea.translate(TAB->boundBox.pos());
            if (!TAB->boundBox.empty()) {
                mappedArea.w = TAB->boundBox.w;
                mappedArea.h = TAB->boundBox.h;
            }
            break;
        }
        case HID_TYPE_TOUCH: {
            ITouch* TOUCH = reinterpret_cast<ITouch*>(dev.get());
            if (!TOUCH->boundOutput.empty()) {
                if (const auto PMONITOR = g_pCompositor->getMonitorFromString(TOUCH->boundOutput); PMONITOR) {
                    currentMonitor = PMONITOR->self.lock();
                    mappedArea     = currentMonitor->logicalBox();
                }
            }
            break;
        }
        default: break;
    }

    damageIfSoftware();

    if (std::isnan(abs.x) || std::isnan(abs.y)) {
        pointerPos.x = std::isnan(abs.x) ? pointerPos.x : mappedArea.x + mappedArea.w * abs.x;
        pointerPos.y = std::isnan(abs.y) ? pointerPos.y : mappedArea.y + mappedArea.h * abs.y;
    } else
        pointerPos = mappedArea.pos() + mappedArea.size() * abs;

    onCursorMoved();
    recheckEnteredOutputs();

    damageIfSoftware();
}

void CPointerManager::onMonitorLayoutChange() {
    currentMonitorLayout.monitorBoxes.clear();
    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->isMirror() || !m->m_bEnabled)
            continue;

        currentMonitorLayout.monitorBoxes.emplace_back(CBox{m->vecPosition, m->vecSize});
    }

    damageIfSoftware();

    pointerPos = closestValid(pointerPos);
    updateCursorBackend();
    recheckEnteredOutputs();

    damageIfSoftware();
}

SP<CTexture> CPointerManager::getCurrentCursorTexture() {
    if (!currentCursorImage.pBuffer && (!currentCursorImage.surface || !currentCursorImage.surface->resource()->current.buffer))
        return nullptr;

    if (currentCursorImage.pBuffer) {
        if (!currentCursorImage.pBufferTexture) {
            currentCursorImage.pBufferTexture = wlr_texture_from_buffer(g_pCompositor->m_sWLRRenderer, currentCursorImage.pBuffer);
            currentCursorImage.bufferTex      = makeShared<CTexture>(currentCursorImage.pBufferTexture);
        }
        return currentCursorImage.bufferTex;
    }

    return currentCursorImage.surface->resource()->current.buffer->texture;
}

void CPointerManager::attachPointer(SP<IPointer> pointer) {
    if (!pointer)
        return;

    auto listener = pointerListeners.emplace_back(makeShared<SPointerListener>());

    listener->pointer = pointer;

    // clang-format off
    listener->destroy = pointer->events.destroy.registerListener([this] (std::any d) {
        detachPointer(nullptr);
    });

    listener->motion = pointer->pointerEvents.motion.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SMotionEvent>(e);

        g_pInputManager->onMouseMoved(E);
    });

    listener->motionAbsolute = pointer->pointerEvents.motionAbsolute.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SMotionAbsoluteEvent>(e);

        g_pInputManager->onMouseWarp(E);
    });

    listener->button = pointer->pointerEvents.button.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SButtonEvent>(e);

        g_pInputManager->onMouseButton(E);
    });

    listener->axis = pointer->pointerEvents.axis.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SAxisEvent>(e);

        g_pInputManager->onMouseWheel(E);
    });

    listener->frame = pointer->pointerEvents.frame.registerListener([this] (std::any e) {
        g_pSeatManager->sendPointerFrame();
    });

    listener->swipeBegin = pointer->pointerEvents.swipeBegin.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeBeginEvent>(e);

        g_pInputManager->onSwipeBegin(E);
    });

    listener->swipeEnd = pointer->pointerEvents.swipeEnd.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeEndEvent>(e);

        g_pInputManager->onSwipeEnd(E);
    });

    listener->swipeUpdate = pointer->pointerEvents.swipeUpdate.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeUpdateEvent>(e);

        g_pInputManager->onSwipeUpdate(E);
    });

    listener->pinchBegin = pointer->pointerEvents.pinchBegin.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchBeginEvent>(e);

        PROTO::pointerGestures->pinchBegin(E.timeMs, E.fingers);
    });

    listener->pinchEnd = pointer->pointerEvents.pinchEnd.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchEndEvent>(e);

        PROTO::pointerGestures->pinchEnd(E.timeMs, E.cancelled);
    });

    listener->pinchUpdate = pointer->pointerEvents.pinchUpdate.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchUpdateEvent>(e);

        PROTO::pointerGestures->pinchUpdate(E.timeMs, E.delta, E.scale, E.rotation);
    });

    listener->holdBegin = pointer->pointerEvents.holdBegin.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SHoldBeginEvent>(e);

        PROTO::pointerGestures->holdBegin(E.timeMs, E.fingers);
    });

    listener->holdEnd = pointer->pointerEvents.holdEnd.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SHoldEndEvent>(e);

        PROTO::pointerGestures->holdEnd(E.timeMs, E.cancelled);
    });
    // clang-format on

    Debug::log(LOG, "Attached pointer {} to global", pointer->hlName);
}

void CPointerManager::attachTouch(SP<ITouch> touch) {
    if (!touch)
        return;

    auto listener = touchListeners.emplace_back(makeShared<STouchListener>());

    listener->touch = touch;

    // clang-format off
    listener->destroy = touch->events.destroy.registerListener([this] (std::any d) {
        detachTouch(nullptr);
    });

    listener->down = touch->touchEvents.down.registerListener([this] (std::any e) {
        auto E = std::any_cast<ITouch::SDownEvent>(e);

        g_pInputManager->onTouchDown(E);
    });

    listener->up = touch->touchEvents.up.registerListener([this] (std::any e) {
        auto E = std::any_cast<ITouch::SUpEvent>(e);

        g_pInputManager->onTouchUp(E);
    });

    listener->motion = touch->touchEvents.motion.registerListener([this] (std::any e) {
        auto E = std::any_cast<ITouch::SMotionEvent>(e);

        g_pInputManager->onTouchMove(E);
    });

    listener->cancel = touch->touchEvents.cancel.registerListener([this] (std::any e) {
        //
    });

    listener->frame = touch->touchEvents.frame.registerListener([this] (std::any e) {
        g_pSeatManager->sendTouchFrame();
    });
    // clang-format on

    Debug::log(LOG, "Attached touch {} to global", touch->hlName);
}

void CPointerManager::attachTablet(SP<CTablet> tablet) {
    if (!tablet)
        return;

    auto listener = tabletListeners.emplace_back(makeShared<STabletListener>());

    listener->tablet = tablet;

    // clang-format off
    listener->destroy = tablet->events.destroy.registerListener([this] (std::any d) {
        detachTablet(nullptr);
    });

    listener->axis = tablet->tabletEvents.axis.registerListener([this] (std::any e) {
        auto E = std::any_cast<CTablet::SAxisEvent>(e);

        g_pInputManager->onTabletAxis(E);
    });

    listener->proximity = tablet->tabletEvents.proximity.registerListener([this] (std::any e) {
        auto E = std::any_cast<CTablet::SProximityEvent>(e);

        g_pInputManager->onTabletProximity(E);
    });

    listener->tip = tablet->tabletEvents.tip.registerListener([this] (std::any e) {
        auto E = std::any_cast<CTablet::STipEvent>(e);

        g_pInputManager->onTabletTip(E);
    });

    listener->button = tablet->tabletEvents.button.registerListener([this] (std::any e) {
        auto E = std::any_cast<CTablet::SButtonEvent>(e);

        g_pInputManager->onTabletButton(E);
    });
    // clang-format on

    Debug::log(LOG, "Attached tablet {} to global", tablet->hlName);
}

void CPointerManager::detachPointer(SP<IPointer> pointer) {
    std::erase_if(pointerListeners, [pointer](const auto& e) { return e->pointer.expired() || e->pointer == pointer; });
}

void CPointerManager::detachTouch(SP<ITouch> touch) {
    std::erase_if(touchListeners, [touch](const auto& e) { return e->touch.expired() || e->touch == touch; });
}

void CPointerManager::detachTablet(SP<CTablet> tablet) {
    std::erase_if(tabletListeners, [tablet](const auto& e) { return e->tablet.expired() || e->tablet == tablet; });
}

void CPointerManager::damageCursor(SP<CMonitor> pMonitor) {
    for (auto& mw : monitorStates) {
        if (mw->monitor != pMonitor)
            continue;

        auto b = getCursorBoxGlobal().intersection(pMonitor->logicalBox());

        if (b.empty())
            return;

        g_pHyprRenderer->damageBox(&b);

        return;
    }
}

Vector2D CPointerManager::cursorSizeLogical() {
    return currentCursorImage.size / currentCursorImage.scale;
}
