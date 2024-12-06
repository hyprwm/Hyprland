#include "PointerManager.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/PointerGestures.hpp"
#include "../protocols/RelativePointer.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/IdleNotify.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/core/Seat.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "SeatManager.hpp"
#include <cstring>
#include <gbm.h>

CPointerManager::CPointerManager() {
    hooks.monitorAdded = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any data) {
        auto PMONITOR = std::any_cast<PHLMONITOR>(data);

        onMonitorLayoutChange();

        PMONITOR->events.modeChanged.registerStaticListener([this](void* owner, std::any data) { g_pEventLoopManager->doLater([this]() { onMonitorLayoutChange(); }); }, nullptr);
        PMONITOR->events.disconnect.registerStaticListener([this](void* owner, std::any data) { g_pEventLoopManager->doLater([this]() { onMonitorLayoutChange(); }); }, nullptr);
        PMONITOR->events.destroy.registerStaticListener(
            [this](void* owner, std::any data) {
                if (g_pCompositor && !g_pCompositor->m_bIsShuttingDown)
                    std::erase_if(monitorStates, [](const auto& other) { return other->monitor.expired(); });
            },
            nullptr);
    });

    hooks.monitorPreRender = g_pHookSystem->hookDynamic("preMonitorCommit", [this](void* self, SCallbackInfo& info, std::any data) {
        auto state = stateFor(std::any_cast<PHLMONITOR>(data));
        if (!state)
            return;

        state->cursorRendered = false;
    });
}

void CPointerManager::lockSoftwareAll() {
    for (auto const& state : monitorStates)
        state->softwareLocks++;

    updateCursorBackend();
}

void CPointerManager::unlockSoftwareAll() {
    for (auto const& state : monitorStates)
        state->softwareLocks--;

    updateCursorBackend();
}

void CPointerManager::lockSoftwareForMonitor(PHLMONITOR mon) {
    auto const state = stateFor(mon);
    state->softwareLocks++;

    if (state->softwareLocks == 1)
        updateCursorBackend();
}

void CPointerManager::unlockSoftwareForMonitor(PHLMONITOR mon) {
    auto const state = stateFor(mon);
    state->softwareLocks--;
    if (state->softwareLocks < 0)
        state->softwareLocks = 0;

    if (state->softwareLocks == 0)
        updateCursorBackend();
}

bool CPointerManager::softwareLockedFor(PHLMONITOR mon) {
    auto const state = stateFor(mon);
    return state->softwareLocks > 0 || state->hardwareFailed;
}

Vector2D CPointerManager::position() {
    return pointerPos;
}

bool CPointerManager::hasCursor() {
    return currentCursorImage.pBuffer || currentCursorImage.surface;
}

SP<CPointerManager::SMonitorPointerState> CPointerManager::stateFor(PHLMONITOR mon) {
    auto it = std::find_if(monitorStates.begin(), monitorStates.end(), [mon](const auto& other) { return other->monitor == mon; });
    if (it == monitorStates.end())
        return monitorStates.emplace_back(makeShared<CPointerManager::SMonitorPointerState>(mon));
    return *it;
}

void CPointerManager::setCursorBuffer(SP<Aquamarine::IBuffer> buf, const Vector2D& hotspot, const float& scale) {
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
        currentCursorImage.size    = buf->size;
        currentCursorImage.pBuffer = buf;
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

        surf->resource()->map();

        currentCursorImage.destroySurface = surf->events.destroy.registerListener([this](std::any data) { resetCursorImage(); });
        currentCursorImage.commitSurface  = surf->resource()->events.commit.registerListener([this](std::any data) {
            damageIfSoftware();
            currentCursorImage.size  = currentCursorImage.surface->resource()->current.texture ? currentCursorImage.surface->resource()->current.bufferSize : Vector2D{};
            currentCursorImage.scale = currentCursorImage.surface ? currentCursorImage.surface->resource()->current.scale : 1.F;
            recheckEnteredOutputs();
            updateCursorBackend();
            damageIfSoftware();
        });

        if (surf->resource()->current.texture) {
            currentCursorImage.size = surf->resource()->current.bufferSize;
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

    for (auto const& s : monitorStates) {
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
            if (!s->hardwareFailed && (s->monitor->output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
                setHWCursorBuffer(s, nullptr);

            if (!currentCursorImage.surface)
                continue;

            currentCursorImage.surface->resource()->leave(s->monitor.lock());
        }
    }
}

void CPointerManager::resetCursorImage(bool apply) {
    damageIfSoftware();

    if (currentCursorImage.surface) {
        for (auto const& m : g_pCompositor->m_vMonitors) {
            currentCursorImage.surface->resource()->leave(m);
        }

        currentCursorImage.surface->resource()->unmap();

        currentCursorImage.destroySurface.reset();
        currentCursorImage.commitSurface.reset();
        currentCursorImage.surface.reset();
    } else if (currentCursorImage.pBuffer)
        currentCursorImage.pBuffer = nullptr;

    if (currentCursorImage.bufferTex)
        currentCursorImage.bufferTex = nullptr;

    currentCursorImage.scale   = 1.F;
    currentCursorImage.hotspot = {0, 0};

    for (auto const& s : monitorStates) {
        if (s->monitor.expired() || s->monitor->isMirror() || !s->monitor->m_bEnabled)
            continue;

        s->entered = false;
    }

    if (!apply)
        return;

    for (auto const& ms : monitorStates) {
        if (!ms->monitor || !ms->monitor->m_bEnabled || !ms->monitor->dpmsStatus) {
            Debug::log(TRACE, "Not updating hw cursors: disabled / dpms off display");
            continue;
        }

        if (ms->cursorFrontBuffer) {
            if (ms->monitor->output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER)
                ms->monitor->output->setCursor(nullptr, {});
            ms->cursorFrontBuffer = nullptr;
        }
    }
}

void CPointerManager::updateCursorBackend() {
    const auto CURSORBOX = getCursorBoxGlobal();

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (!m->m_bEnabled || !m->dpmsStatus) {
            Debug::log(TRACE, "Not updating hw cursors: disabled / dpms off display");
            continue;
        }

        auto CROSSES = !m->logicalBox().intersection(CURSORBOX).empty();
        auto state   = stateFor(m);

        if (!CROSSES) {
            if (state->cursorFrontBuffer)
                setHWCursorBuffer(state, nullptr);

            continue;
        }

        if (state->softwareLocks > 0 || g_pConfigManager->shouldUseSoftwareCursors() || !attemptHardwareCursor(state)) {
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

    const auto CURSORBOX = getCursorBoxGlobal();
    bool       recalc    = false;

    for (auto const& m : g_pCompositor->m_vMonitors) {
        auto state = stateFor(m);

        state->box = getCursorBoxLogicalForMonitor(state->monitor.lock());

        auto CROSSES = !m->logicalBox().intersection(CURSORBOX).empty();

        if (!CROSSES && state->cursorFrontBuffer) {
            Debug::log(TRACE, "onCursorMoved for output {}: cursor left the viewport, removing it from the backend", m->szName);
            setHWCursorBuffer(state, nullptr);
            continue;
        } else if (CROSSES && !state->cursorFrontBuffer) {
            Debug::log(TRACE, "onCursorMoved for output {}: cursor entered the output, but no front buffer, forcing recalc", m->szName);
            recalc = true;
        }

        if (state->hardwareFailed || !state->entered)
            continue;

        const auto CURSORPOS = getCursorPosForMonitor(m);
        m->output->moveCursor(CURSORPOS, m->shouldSkipScheduleFrameOnMouseEvent());
    }

    if (recalc)
        updateCursorBackend();
}

bool CPointerManager::attemptHardwareCursor(SP<CPointerManager::SMonitorPointerState> state) {
    auto output = state->monitor->output;

    if (!(output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
        return false;

    const auto CURSORPOS = getCursorPosForMonitor(state->monitor.lock());
    state->monitor->output->moveCursor(CURSORPOS, state->monitor->shouldSkipScheduleFrameOnMouseEvent());

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

bool CPointerManager::setHWCursorBuffer(SP<SMonitorPointerState> state, SP<Aquamarine::IBuffer> buf) {
    if (!(state->monitor->output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
        return false;

    const auto HOTSPOT = transformedHotspot(state->monitor.lock());

    Debug::log(TRACE, "[pointer] hw transformed hotspot for {}: {}", state->monitor->szName, HOTSPOT);

    if (!state->monitor->output->setCursor(buf, HOTSPOT))
        return false;

    state->cursorFrontBuffer = buf;

    if (!state->monitor->shouldSkipScheduleFrameOnMouseEvent())
        g_pCompositor->scheduleFrameForMonitor(state->monitor.lock(), Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_SHAPE);

    return true;
}

SP<Aquamarine::IBuffer> CPointerManager::renderHWCursorBuffer(SP<CPointerManager::SMonitorPointerState> state, SP<CTexture> texture) {
    auto        maxSize    = state->monitor->output->cursorPlaneSize();
    auto const& cursorSize = currentCursorImage.size;

    static auto PDUMB = CConfigValue<Hyprlang::INT>("cursor:use_cpu_buffer");

    if (maxSize == Vector2D{})
        return nullptr;

    if (maxSize != Vector2D{-1, -1}) {
        if (cursorSize.x > maxSize.x || cursorSize.y > maxSize.y) {
            Debug::log(TRACE, "hardware cursor too big! {} > {}", currentCursorImage.size, maxSize);
            return nullptr;
        }
    } else
        maxSize = cursorSize;

    if (!state->monitor->cursorSwapchain || maxSize != state->monitor->cursorSwapchain->currentOptions().size ||
        *PDUMB != (state->monitor->cursorSwapchain->getAllocator()->type() != Aquamarine::AQ_ALLOCATOR_TYPE_GBM)) {

        if (!state->monitor->cursorSwapchain || *PDUMB != (state->monitor->cursorSwapchain->getAllocator()->type() != Aquamarine::AQ_ALLOCATOR_TYPE_GBM)) {

            auto allocator = state->monitor->output->getBackend()->preferredAllocator();
            if (*PDUMB) {
                for (const auto& a : state->monitor->output->getBackend()->getAllocators()) {
                    if (a->type() == Aquamarine::AQ_ALLOCATOR_TYPE_DRM_DUMB) {
                        allocator = a;
                        break;
                    }
                }
            }

            state->monitor->cursorSwapchain = Aquamarine::CSwapchain::create(allocator, state->monitor->output->getBackend());
        }

        auto options     = state->monitor->cursorSwapchain->currentOptions();
        options.size     = maxSize;
        options.length   = 2;
        options.scanout  = true;
        options.cursor   = true;
        options.multigpu = state->monitor->output->getBackend()->preferredAllocator()->drmFD() != g_pCompositor->m_iDRMFD;
        // We do not set the format (unless shm). If it's unset (DRM_FORMAT_INVALID) then the swapchain will pick for us,
        // but if it's set, we don't wanna change it.
        if (*PDUMB)
            options.format = DRM_FORMAT_ARGB8888;

        if (!state->monitor->cursorSwapchain->reconfigure(options)) {
            Debug::log(TRACE, "Failed to reconfigure cursor swapchain");
            return nullptr;
        }
    }

    // if we already rendered the cursor, revert the swapchain to avoid rendering the cursor over
    // the current front buffer
    // this flag will be reset in the preRender hook, so when we commit this buffer to KMS
    if (state->cursorRendered)
        state->monitor->cursorSwapchain->rollback();

    state->cursorRendered = true;

    auto buf = state->monitor->cursorSwapchain->next(nullptr);
    if (!buf) {
        Debug::log(TRACE, "Failed to acquire a buffer from the cursor swapchain");
        return nullptr;
    }

    if (*PDUMB) {
        // get the texture data if available.
        auto texData = texture->dataCopy();
        if (texData.empty()) {
            if (currentCursorImage.surface && currentCursorImage.surface->resource()->role->role() == SURFACE_ROLE_CURSOR) {
                const auto SURFACE   = currentCursorImage.surface->resource();
                auto&      shmBuffer = CCursorSurfaceRole::cursorPixelData(SURFACE);

                bool       flipRB = false;

                if (SURFACE->current.texture) {
                    Debug::log(TRACE, "Cursor CPU surface: format {}, expecting AR24", NFormatUtils::drmFormatName(SURFACE->current.texture->m_iDrmFormat));
                    if (SURFACE->current.texture->m_iDrmFormat == DRM_FORMAT_ABGR8888) {
                        Debug::log(TRACE, "Cursor CPU surface format AB24, will flip. WARNING: this will break on big endian!");
                        flipRB = true;
                    } else if (SURFACE->current.texture->m_iDrmFormat != DRM_FORMAT_ARGB8888) {
                        Debug::log(TRACE, "Cursor CPU surface format rejected, falling back to sw");
                        return nullptr;
                    }
                }

                if (shmBuffer.data())
                    texData = shmBuffer;
                else {
                    texData.resize(texture->m_vSize.x * 4 * texture->m_vSize.y);
                    memset(texData.data(), 0x00, texData.size());
                }

                if (flipRB) {
                    for (size_t i = 0; i < shmBuffer.size(); i += 4) {
                        std::swap(shmBuffer.at(i), shmBuffer.at(i + 2)); // little-endian!!!!!!
                    }
                }
            } else {
                Debug::log(TRACE, "Cannot use dumb copy on dmabuf cursor buffers");
                return nullptr;
            }
        }

        // then, we just yeet it into the dumb buffer

        auto [data, fmt, size] = buf->beginDataPtr(0);

        memset(data, 0, size);
        if (buf->dmabuf().size.x > texture->m_vSize.x) {
            size_t STRIDE = 4 * texture->m_vSize.x;
            for (int i = 0; i < texture->m_vSize.y; i++)
                memcpy(data + i * buf->dmabuf().strides[0], texData.data() + i * STRIDE, STRIDE);
        } else
            memcpy(data, texData.data(), std::min(size, texData.size()));

        buf->endDataPtr();

        return buf;
    }

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_RenderData.pMonitor = state->monitor;

    auto RBO = g_pHyprRenderer->getOrCreateRenderbuffer(buf, state->monitor->cursorSwapchain->currentOptions().format);
    if (!RBO) {
        Debug::log(TRACE, "Failed to create cursor RB with format {}, mod {}", buf->dmabuf().format, buf->dmabuf().modifier);
        return nullptr;
    }

    RBO->bind();

    g_pHyprOpenGL->beginSimple(state->monitor.lock(), {0, 0, INT16_MAX, INT16_MAX}, RBO);
    g_pHyprOpenGL->clear(CHyprColor{0.F, 0.F, 0.F, 0.F});

    CBox xbox = {{}, Vector2D{currentCursorImage.size / currentCursorImage.scale * state->monitor->scale}.round()};
    Debug::log(TRACE, "[pointer] monitor: {}, size: {}, hw buf: {}, scale: {:.2f}, monscale: {:.2f}, xbox: {}", state->monitor->szName, currentCursorImage.size, cursorSize,
               currentCursorImage.scale, state->monitor->scale, xbox.size());

    g_pHyprOpenGL->renderTexture(texture, &xbox, 1.F);

    g_pHyprOpenGL->end();
    glFlush();
    g_pHyprOpenGL->m_RenderData.pMonitor.reset();

    g_pHyprRenderer->onRenderbufferDestroy(RBO.get());

    return buf;
}

void CPointerManager::renderSoftwareCursorsFor(PHLMONITOR pMonitor, timespec* now, CRegion& damage, std::optional<Vector2D> overridePos) {
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
    box.x = std::round(box.x);
    box.y = std::round(box.y);

    g_pHyprOpenGL->renderTextureWithDamage(texture, &box, &damage, 1.F, 0, false, false, currentCursorImage.waitTimeline, currentCursorImage.waitPoint);

    if (currentCursorImage.surface)
        currentCursorImage.surface->resource()->frame(now);
}

Vector2D CPointerManager::getCursorPosForMonitor(PHLMONITOR pMonitor) {
    return CBox{pointerPos - pMonitor->vecPosition, {0, 0}}
               .transform(wlTransformToHyprutils(invertTransform(pMonitor->transform)), pMonitor->vecTransformedSize.x / pMonitor->scale,
                          pMonitor->vecTransformedSize.y / pMonitor->scale)
               .pos() *
        pMonitor->scale;
}

Vector2D CPointerManager::transformedHotspot(PHLMONITOR pMonitor) {
    if (!pMonitor->cursorSwapchain)
        return {}; // doesn't matter, we have no hw cursor, and this is only for hw cursors

    return CBox{currentCursorImage.hotspot * pMonitor->scale, {0, 0}}
        .transform(wlTransformToHyprutils(invertTransform(pMonitor->transform)), pMonitor->cursorSwapchain->currentOptions().size.x,
                   pMonitor->cursorSwapchain->currentOptions().size.y)
        .pos();
}

CBox CPointerManager::getCursorBoxLogicalForMonitor(PHLMONITOR pMonitor) {
    return getCursorBoxGlobal().translate(-pMonitor->vecPosition);
}

CBox CPointerManager::getCursorBoxGlobal() {
    return CBox{pointerPos, currentCursorImage.size / currentCursorImage.scale}.translate(-currentCursorImage.hotspot);
}

Vector2D CPointerManager::closestValid(const Vector2D& pos) {
    static auto PADDING = CConfigValue<Hyprlang::INT>("cursor:hotspot_padding");

    auto        CURSOR_PADDING = std::clamp((int)*PADDING, 0, 100);
    CBox        hotBox         = {{pos.x - CURSOR_PADDING, pos.y - CURSOR_PADDING}, {2 * CURSOR_PADDING, 2 * CURSOR_PADDING}};

    //
    static auto INSIDE_LAYOUT = [this](const CBox& box) -> bool {
        for (auto const& b : currentMonitorLayout.monitorBoxes) {
            if (box.inside(b))
                return true;
        }
        return false;
    };

    static auto INSIDE_LAYOUT_COORD = [this](const Vector2D& vec) -> bool {
        for (auto const& b : currentMonitorLayout.monitorBoxes) {
            if (b.containsPoint(vec))
                return true;
        }
        return false;
    };

    static auto NEAREST_LAYOUT = [this](const Vector2D& vec) -> Vector2D {
        Vector2D leader;
        float    distanceSq = __FLT_MAX__;

        for (auto const& b : currentMonitorLayout.monitorBoxes) {
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
    auto b = getCursorBoxGlobal().expand(4);

    for (auto const& mw : monitorStates) {
        if (mw->monitor.expired() || !mw->monitor->output)
            continue;

        if ((mw->softwareLocks > 0 || mw->hardwareFailed || g_pConfigManager->shouldUseSoftwareCursors()) && b.overlaps({mw->monitor->vecPosition, mw->monitor->vecSize})) {
            g_pHyprRenderer->damageBox(&b, mw->monitor->shouldSkipScheduleFrameOnMouseEvent());
            break;
        }
    }
}

void CPointerManager::warpTo(const Vector2D& logical) {
    damageIfSoftware();

    pointerPos = closestValid(logical);

    if (!g_pInputManager->isLocked()) {
        recheckEnteredOutputs();
        onCursorMoved();
    }

    damageIfSoftware();
}

void CPointerManager::move(const Vector2D& deltaLogical) {
    const auto oldPos = pointerPos;
    auto       newPos = oldPos + Vector2D{std::isnan(deltaLogical.x) ? 0.0 : deltaLogical.x, std::isnan(deltaLogical.y) ? 0.0 : deltaLogical.y};

    warpTo(newPos);
}

void CPointerManager::warpAbsolute(Vector2D abs, SP<IHID> dev) {
    if (!dev)
        return;

    if (!std::isnan(abs.x))
        abs.x = std::clamp(abs.x, 0.0, 1.0);
    if (!std::isnan(abs.y))
        abs.y = std::clamp(abs.y, 0.0, 1.0);

    // find x and y size of the entire space
    const auto& MONITORS = g_pCompositor->m_vMonitors;
    Vector2D    topLeft = MONITORS.at(0)->vecPosition, bottomRight = MONITORS.at(0)->vecPosition + MONITORS.at(0)->vecSize;
    for (size_t i = 1; i < MONITORS.size(); ++i) {
        const auto EXTENT = MONITORS[i]->logicalBox().extent();
        const auto POS    = MONITORS[i]->logicalBox().pos();
        if (EXTENT.x > bottomRight.x)
            bottomRight.x = EXTENT.x;
        if (EXTENT.y > bottomRight.y)
            bottomRight.y = EXTENT.y;
        if (POS.x < topLeft.x)
            topLeft.x = POS.x;
        if (POS.y < topLeft.y)
            topLeft.y = POS.y;
    }
    CBox mappedArea = {topLeft, bottomRight - topLeft};

    auto outputMappedArea = [&mappedArea](const std::string& output) {
        if (output == "current") {
            if (const auto PLASTMONITOR = g_pCompositor->m_pLastMonitor.lock(); PLASTMONITOR)
                return PLASTMONITOR->logicalBox();
        } else if (const auto PMONITOR = g_pCompositor->getMonitorFromString(output); PMONITOR)
            return PMONITOR->logicalBox();
        return mappedArea;
    };

    switch (dev->getType()) {
        case HID_TYPE_TABLET: {
            CTablet* TAB = reinterpret_cast<CTablet*>(dev.get());
            if (!TAB->boundOutput.empty()) {
                mappedArea = outputMappedArea(TAB->boundOutput);
                mappedArea.translate(TAB->boundBox.pos());
            } else if (TAB->absolutePos) {
                mappedArea.x = TAB->boundBox.x;
                mappedArea.y = TAB->boundBox.y;
            } else
                mappedArea.translate(TAB->boundBox.pos());

            if (!TAB->boundBox.empty()) {
                mappedArea.w = TAB->boundBox.w;
                mappedArea.h = TAB->boundBox.h;
            }
            break;
        }
        case HID_TYPE_TOUCH: {
            ITouch* TOUCH = reinterpret_cast<ITouch*>(dev.get());
            if (!TOUCH->boundOutput.empty())
                mappedArea = outputMappedArea(TOUCH->boundOutput);
            break;
        }
        case HID_TYPE_POINTER: {
            IPointer* POINTER = reinterpret_cast<IPointer*>(dev.get());
            if (!POINTER->boundOutput.empty())
                mappedArea = outputMappedArea(POINTER->boundOutput);
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
    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (m->isMirror() || !m->m_bEnabled || !m->output)
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
    if (!currentCursorImage.pBuffer && (!currentCursorImage.surface || !currentCursorImage.surface->resource()->current.texture))
        return nullptr;

    if (currentCursorImage.pBuffer) {
        if (!currentCursorImage.bufferTex)
            currentCursorImage.bufferTex = makeShared<CTexture>(currentCursorImage.pBuffer, true);
        return currentCursorImage.bufferTex;
    }

    return currentCursorImage.surface->resource()->current.texture;
}

void CPointerManager::attachPointer(SP<IPointer> pointer) {
    if (!pointer)
        return;

    static auto PMOUSEDPMS = CConfigValue<Hyprlang::INT>("misc:mouse_move_enables_dpms");

    //
    auto listener = pointerListeners.emplace_back(makeShared<SPointerListener>());

    listener->pointer = pointer;

    // clang-format off
    listener->destroy = pointer->events.destroy.registerListener([this] (std::any d) {
        detachPointer(nullptr);
    });

    listener->motion = pointer->pointerEvents.motion.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SMotionEvent>(e);

        g_pInputManager->onMouseMoved(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_bDPMSStateON && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->motionAbsolute = pointer->pointerEvents.motionAbsolute.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SMotionAbsoluteEvent>(e);

        g_pInputManager->onMouseWarp(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_bDPMSStateON && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->button = pointer->pointerEvents.button.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SButtonEvent>(e);

        g_pInputManager->onMouseButton(E);

        PROTO::idle->onActivity();
    });

    listener->axis = pointer->pointerEvents.axis.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SAxisEvent>(e);

        g_pInputManager->onMouseWheel(E);

        PROTO::idle->onActivity();
    });

    listener->frame = pointer->pointerEvents.frame.registerListener([this] (std::any e) {
        bool shouldSkip = false;
        if (!g_pSeatManager->mouse.expired() && g_pInputManager->isLocked()) {
            auto PMONITOR = g_pCompositor->m_pLastMonitor.get();
            shouldSkip = PMONITOR && PMONITOR->shouldSkipScheduleFrameOnMouseEvent();
        }
        g_pSeatManager->isPointerFrameSkipped = shouldSkip;
        if (!g_pSeatManager->isPointerFrameSkipped)
            g_pSeatManager->sendPointerFrame();
    });

    listener->swipeBegin = pointer->pointerEvents.swipeBegin.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeBeginEvent>(e);

        g_pInputManager->onSwipeBegin(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_bDPMSStateON && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->swipeEnd = pointer->pointerEvents.swipeEnd.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeEndEvent>(e);

        g_pInputManager->onSwipeEnd(E);

        PROTO::idle->onActivity();
    });

    listener->swipeUpdate = pointer->pointerEvents.swipeUpdate.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeUpdateEvent>(e);

        g_pInputManager->onSwipeUpdate(E);

        PROTO::idle->onActivity();
    });

    listener->pinchBegin = pointer->pointerEvents.pinchBegin.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchBeginEvent>(e);

        PROTO::pointerGestures->pinchBegin(E.timeMs, E.fingers);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_bDPMSStateON && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->pinchEnd = pointer->pointerEvents.pinchEnd.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchEndEvent>(e);

        PROTO::pointerGestures->pinchEnd(E.timeMs, E.cancelled);

        PROTO::idle->onActivity();
    });

    listener->pinchUpdate = pointer->pointerEvents.pinchUpdate.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchUpdateEvent>(e);

        PROTO::pointerGestures->pinchUpdate(E.timeMs, E.delta, E.scale, E.rotation);

        PROTO::idle->onActivity();
    });

    listener->holdBegin = pointer->pointerEvents.holdBegin.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SHoldBeginEvent>(e);

        PROTO::pointerGestures->holdBegin(E.timeMs, E.fingers);

        PROTO::idle->onActivity();
    });

    listener->holdEnd = pointer->pointerEvents.holdEnd.registerListener([this] (std::any e) {
        auto E = std::any_cast<IPointer::SHoldEndEvent>(e);

        PROTO::pointerGestures->holdEnd(E.timeMs, E.cancelled);

        PROTO::idle->onActivity();
    });
    // clang-format on

    Debug::log(LOG, "Attached pointer {} to global", pointer->hlName);
}

void CPointerManager::attachTouch(SP<ITouch> touch) {
    if (!touch)
        return;

    static auto PMOUSEDPMS = CConfigValue<Hyprlang::INT>("misc:mouse_move_enables_dpms");

    //
    auto listener = touchListeners.emplace_back(makeShared<STouchListener>());

    listener->touch = touch;

    // clang-format off
    listener->destroy = touch->events.destroy.registerListener([this] (std::any d) {
        detachTouch(nullptr);
    });

    listener->down = touch->touchEvents.down.registerListener([this] (std::any e) {
        auto E = std::any_cast<ITouch::SDownEvent>(e);

        g_pInputManager->onTouchDown(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_bDPMSStateON && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->up = touch->touchEvents.up.registerListener([this] (std::any e) {
        auto E = std::any_cast<ITouch::SUpEvent>(e);

        g_pInputManager->onTouchUp(E);

        PROTO::idle->onActivity();
    });

    listener->motion = touch->touchEvents.motion.registerListener([this] (std::any e) {
        auto E = std::any_cast<ITouch::SMotionEvent>(e);

        g_pInputManager->onTouchMove(E);

        PROTO::idle->onActivity();
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

    static auto PMOUSEDPMS = CConfigValue<Hyprlang::INT>("misc:mouse_move_enables_dpms");

    //
    auto listener = tabletListeners.emplace_back(makeShared<STabletListener>());

    listener->tablet = tablet;

    // clang-format off
    listener->destroy = tablet->events.destroy.registerListener([this] (std::any d) {
        detachTablet(nullptr);
    });

    listener->axis = tablet->tabletEvents.axis.registerListener([this] (std::any e) {
        auto E = std::any_cast<CTablet::SAxisEvent>(e);

        g_pInputManager->onTabletAxis(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_bDPMSStateON && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->proximity = tablet->tabletEvents.proximity.registerListener([this] (std::any e) {
        auto E = std::any_cast<CTablet::SProximityEvent>(e);

        g_pInputManager->onTabletProximity(E);

        PROTO::idle->onActivity();
    });

    listener->tip = tablet->tabletEvents.tip.registerListener([this] (std::any e) {
        auto E = std::any_cast<CTablet::STipEvent>(e);

        g_pInputManager->onTabletTip(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_bDPMSStateON && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->button = tablet->tabletEvents.button.registerListener([this] (std::any e) {
        auto E = std::any_cast<CTablet::SButtonEvent>(e);

        g_pInputManager->onTabletButton(E);

        PROTO::idle->onActivity();
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

void CPointerManager::damageCursor(PHLMONITOR pMonitor) {
    for (auto const& mw : monitorStates) {
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

void CPointerManager::storeMovement(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel) {
    storedTime = time;
    storedDelta += delta;
    storedUnaccel += deltaUnaccel;
}

void CPointerManager::setStoredMovement(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel) {
    storedTime    = time;
    storedDelta   = delta;
    storedUnaccel = deltaUnaccel;
}

void CPointerManager::sendStoredMovement() {
    PROTO::relativePointer->sendRelativeMotion((uint64_t)storedTime * 1000, storedDelta, storedUnaccel);
    storedTime    = 0;
    storedDelta   = Vector2D{};
    storedUnaccel = Vector2D{};
}
