#include "PointerManager.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/PointerGestures.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/core/Compositor.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "SeatManager.hpp"

CPointerManager::CPointerManager() {
    hooks.monitorAdded = g_pHookSystem->hookDynamic("newMonitor", [this](void* self, SCallbackInfo& info, std::any data) {
        auto PMONITOR = std::any_cast<SP<CMonitor>>(data);

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
}

void CPointerManager::lockSoftwareAll() {
    for (auto& state : monitorStates)
        state->softwareLocks++;

    updateCursorBackend();
}

void CPointerManager::unlockSoftwareAll() {
    for (auto& state : monitorStates)
        state->softwareLocks--;

    updateCursorBackend();
}

void CPointerManager::lockSoftwareForMonitor(CMonitor* Monitor) {
    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->ID == Monitor->ID) {
            lockSoftwareForMonitor(m);
            return;
        }
    }
}

void CPointerManager::lockSoftwareForMonitor(SP<CMonitor> mon) {
    auto state = stateFor(mon);
    state->softwareLocks++;

    if (state->softwareLocks == 1)
        updateCursorBackend();
}

void CPointerManager::unlockSoftwareForMonitor(CMonitor* Monitor) {
    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->ID == Monitor->ID) {
            unlockSoftwareForMonitor(m);
            return;
        }
    }
}

void CPointerManager::unlockSoftwareForMonitor(SP<CMonitor> mon) {
    auto state = stateFor(mon);
    state->softwareLocks--;
    if (state->softwareLocks < 0)
        state->softwareLocks = 0;

    if (state->softwareLocks == 0)
        updateCursorBackend();
}

bool CPointerManager::softwareLockedFor(SP<CMonitor> mon) {
    auto state = stateFor(mon);
    return state->softwareLocks > 0 || state->hardwareFailed;
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
            if (!s->hardwareFailed && (s->monitor->output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
                s->monitor->output->moveCursor({-1337, -420});

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
    } else if (currentCursorImage.pBuffer)
        currentCursorImage.pBuffer = nullptr;

    if (currentCursorImage.bufferTex)
        currentCursorImage.bufferTex = nullptr;

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
            if (ms->monitor->output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER)
                ms->monitor->output->setCursor(nullptr, {});
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
        m->output->moveCursor(CURSORPOS);
    }
}

bool CPointerManager::attemptHardwareCursor(SP<CPointerManager::SMonitorPointerState> state) {
    auto output = state->monitor->output;

    if (!(output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
        return false;

    const auto CURSORPOS = getCursorPosForMonitor(state->monitor.lock());
    state->monitor->output->moveCursor(CURSORPOS);

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

    g_pCompositor->scheduleFrameForMonitor(state->monitor.get());

    return true;
}

SP<Aquamarine::IBuffer> CPointerManager::renderHWCursorBuffer(SP<CPointerManager::SMonitorPointerState> state, SP<CTexture> texture) {
    auto output = state->monitor->output;

    auto maxSize    = output->cursorPlaneSize();
    auto cursorSize = currentCursorImage.size;

    if (maxSize == Vector2D{})
        return nullptr;

    if (maxSize != Vector2D{-1, -1}) {
        if (cursorSize.x > maxSize.x || cursorSize.y > maxSize.y) {
            Debug::log(TRACE, "hardware cursor too big! {} > {}", currentCursorImage.size, maxSize);
            return nullptr;
        }
    } else
        maxSize = cursorSize;

    if (!state->monitor->cursorSwapchain || maxSize != state->monitor->cursorSwapchain->currentOptions().size) {

        if (!state->monitor->cursorSwapchain)
            state->monitor->cursorSwapchain = Aquamarine::CSwapchain::create(g_pCompositor->m_pAqBackend->allocator, state->monitor->output->getBackend());

        auto options    = state->monitor->cursorSwapchain->currentOptions();
        options.size    = maxSize;
        options.length  = 2;
        options.scanout = true;
        options.cursor  = true;
        // We do not set the format. If it's unset (DRM_FORMAT_INVALID) then the swapchain will pick for us,
        // but if it's set, we don't wanna change it.

        if (!state->monitor->cursorSwapchain->reconfigure(options)) {
            Debug::log(TRACE, "Failed to reconfigure cursor swapchain");
            return nullptr;
        }
    }

    auto buf = state->monitor->cursorSwapchain->next(nullptr);
    if (!buf) {
        Debug::log(TRACE, "Failed to acquire a buffer from the cursor swapchain");
        return nullptr;
    }

    CRegion damage = {0, 0, INT16_MAX, INT16_MAX};

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_RenderData.pMonitor = state->monitor.get();

    const auto RBO = g_pHyprRenderer->getOrCreateRenderbuffer(buf, state->monitor->cursorSwapchain->currentOptions().format);
    RBO->bind();

    g_pHyprOpenGL->beginSimple(state->monitor.get(), damage, RBO);
    g_pHyprOpenGL->clear(CColor{0.F, 0.F, 0.F, 0.F});

    CBox xbox = {{}, Vector2D{currentCursorImage.size / currentCursorImage.scale * state->monitor->scale}.round()};
    Debug::log(TRACE, "[pointer] monitor: {}, size: {}, hw buf: {}, scale: {:.2f}, monscale: {:.2f}, xbox: {}", state->monitor->szName, currentCursorImage.size, cursorSize,
               currentCursorImage.scale, state->monitor->scale, xbox.size());

    g_pHyprOpenGL->renderTexture(texture, &xbox, 1.F);

    g_pHyprOpenGL->end();
    glFlush();
    g_pHyprOpenGL->m_RenderData.pMonitor = nullptr;

    g_pHyprRenderer->onRenderbufferDestroy(RBO);

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
    box.x = std::round(box.x);
    box.y = std::round(box.y);

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
    if (!pMonitor->cursorSwapchain)
        return {}; // doesn't matter, we have no hw cursor, and this is only for hw cursors

    return CBox{currentCursorImage.hotspot * pMonitor->scale, {0, 0}}
        .transform(wlTransformToHyprutils(invertTransform(pMonitor->transform)), pMonitor->cursorSwapchain->currentOptions().size.x,
                   pMonitor->cursorSwapchain->currentOptions().size.y)
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

    auto        CURSOR_PADDING = std::clamp((int)*PADDING, 0, 100);
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
            g_pHyprRenderer->damageBox(&b, mw->monitor->shouldSkipScheduleFrameOnMouseEvent());
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
        if (!currentCursorImage.bufferTex)
            currentCursorImage.bufferTex = makeShared<CTexture>(currentCursorImage.pBuffer);
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
