#include "PointerManager.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../protocols/PointerGestures.hpp"
#include "../protocols/RelativePointer.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/IdleNotify.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/core/Seat.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../render/Renderer.hpp"
#include "../render/OpenGL.hpp"
#include "SeatManager.hpp"
#include "../helpers/time/Time.hpp"
#include <cstring>
#include <gbm.h>
#include <cairo/cairo.h>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

CPointerManager::CPointerManager() {
    m_hooks.monitorAdded = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any data) {
        auto PMONITOR = std::any_cast<PHLMONITOR>(data);

        onMonitorLayoutChange();

        PMONITOR->m_events.modeChanged.registerStaticListener([this](void* owner, std::any data) { g_pEventLoopManager->doLater([this]() { onMonitorLayoutChange(); }); }, nullptr);
        PMONITOR->m_events.disconnect.registerStaticListener([this](void* owner, std::any data) { g_pEventLoopManager->doLater([this]() { onMonitorLayoutChange(); }); }, nullptr);
        PMONITOR->m_events.destroy.registerStaticListener(
            [this](void* owner, std::any data) {
                if (g_pCompositor && !g_pCompositor->m_isShuttingDown)
                    std::erase_if(m_monitorStates, [](const auto& other) { return other->monitor.expired(); });
            },
            nullptr);
    });

    m_hooks.monitorPreRender = g_pHookSystem->hookDynamic("preMonitorCommit", [this](void* self, SCallbackInfo& info, std::any data) {
        auto state = stateFor(std::any_cast<PHLMONITOR>(data));
        if (!state)
            return;

        state->cursorRendered = false;
    });
}

void CPointerManager::lockSoftwareAll() {
    for (auto const& state : m_monitorStates)
        state->softwareLocks++;

    updateCursorBackend();
}

void CPointerManager::unlockSoftwareAll() {
    for (auto const& state : m_monitorStates)
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
    return m_pointerPos;
}

bool CPointerManager::hasCursor() {
    return m_currentCursorImage.pBuffer || m_currentCursorImage.surface;
}

SP<CPointerManager::SMonitorPointerState> CPointerManager::stateFor(PHLMONITOR mon) {
    auto it = std::find_if(m_monitorStates.begin(), m_monitorStates.end(), [mon](const auto& other) { return other->monitor == mon; });
    if (it == m_monitorStates.end())
        return m_monitorStates.emplace_back(makeShared<CPointerManager::SMonitorPointerState>(mon));
    return *it;
}

void CPointerManager::setCursorBuffer(SP<Aquamarine::IBuffer> buf, const Vector2D& hotspot, const float& scale) {
    damageIfSoftware();
    if (buf == m_currentCursorImage.pBuffer) {
        if (hotspot != m_currentCursorImage.hotspot || scale != m_currentCursorImage.scale) {
            m_currentCursorImage.hotspot = hotspot;
            m_currentCursorImage.scale   = scale;
            updateCursorBackend();
            damageIfSoftware();
        }

        return;
    }

    resetCursorImage(false);

    if (buf) {
        m_currentCursorImage.size    = buf->size;
        m_currentCursorImage.pBuffer = buf;
    }

    m_currentCursorImage.hotspot = hotspot;
    m_currentCursorImage.scale   = scale;

    updateCursorBackend();
    damageIfSoftware();
}

void CPointerManager::setCursorSurface(SP<CWLSurface> surf, const Vector2D& hotspot) {
    damageIfSoftware();

    if (surf == m_currentCursorImage.surface) {
        if (hotspot != m_currentCursorImage.hotspot || (surf && surf->resource() ? surf->resource()->m_current.scale : 1.F) != m_currentCursorImage.scale) {
            m_currentCursorImage.hotspot = hotspot;
            m_currentCursorImage.scale   = surf && surf->resource() ? surf->resource()->m_current.scale : 1.F;
            updateCursorBackend();
            damageIfSoftware();
        }

        return;
    }

    resetCursorImage(false);

    if (surf) {
        m_currentCursorImage.surface = surf;
        m_currentCursorImage.scale   = surf->resource()->m_current.scale;

        surf->resource()->map();

        m_currentCursorImage.destroySurface = surf->m_events.destroy.registerListener([this](std::any data) { resetCursorImage(); });
        m_currentCursorImage.commitSurface  = surf->resource()->m_events.commit.registerListener([this](std::any data) {
            damageIfSoftware();
            m_currentCursorImage.size  = m_currentCursorImage.surface->resource()->m_current.texture ? m_currentCursorImage.surface->resource()->m_current.bufferSize : Vector2D{};
            m_currentCursorImage.scale = m_currentCursorImage.surface ? m_currentCursorImage.surface->resource()->m_current.scale : 1.F;
            recheckEnteredOutputs();
            updateCursorBackend();
            damageIfSoftware();
        });

        if (surf->resource()->m_current.texture) {
            m_currentCursorImage.size = surf->resource()->m_current.bufferSize;
            surf->resource()->frame(Time::steadyNow());
        }
    }

    m_currentCursorImage.hotspot = hotspot;

    recheckEnteredOutputs();
    updateCursorBackend();
    damageIfSoftware();
}

void CPointerManager::recheckEnteredOutputs() {
    if (!hasCursor())
        return;

    auto box = getCursorBoxGlobal();

    for (auto const& s : m_monitorStates) {
        if (s->monitor.expired() || s->monitor->isMirror() || !s->monitor->m_enabled)
            continue;

        const bool overlaps = box.overlaps(s->monitor->logicalBox());

        if (!s->entered && overlaps) {
            s->entered = true;

            if (!m_currentCursorImage.surface)
                continue;

            m_currentCursorImage.surface->resource()->enter(s->monitor.lock());
            PROTO::fractional->sendScale(m_currentCursorImage.surface->resource(), s->monitor->m_scale);
            g_pCompositor->setPreferredScaleForSurface(m_currentCursorImage.surface->resource(), s->monitor->m_scale);
        } else if (s->entered && !overlaps) {
            s->entered = false;

            // if we are using hw cursors, prevent
            // the cursor from being stuck at the last point.
            if (!s->hardwareFailed &&
                (s->monitor->m_output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
                setHWCursorBuffer(s, nullptr);

            if (!m_currentCursorImage.surface)
                continue;

            m_currentCursorImage.surface->resource()->leave(s->monitor.lock());
        }
    }
}

void CPointerManager::resetCursorImage(bool apply) {
    damageIfSoftware();

    if (m_currentCursorImage.surface) {
        for (auto const& m : g_pCompositor->m_monitors) {
            m_currentCursorImage.surface->resource()->leave(m);
        }

        m_currentCursorImage.surface->resource()->unmap();

        m_currentCursorImage.destroySurface.reset();
        m_currentCursorImage.commitSurface.reset();
        m_currentCursorImage.surface.reset();
    } else if (m_currentCursorImage.pBuffer)
        m_currentCursorImage.pBuffer = nullptr;

    if (m_currentCursorImage.bufferTex)
        m_currentCursorImage.bufferTex = nullptr;

    m_currentCursorImage.scale   = 1.F;
    m_currentCursorImage.hotspot = {0, 0};

    for (auto const& s : m_monitorStates) {
        if (s->monitor.expired() || s->monitor->isMirror() || !s->monitor->m_enabled)
            continue;

        s->entered = false;
    }

    if (!apply)
        return;

    for (auto const& ms : m_monitorStates) {
        if (!ms->monitor || !ms->monitor->m_enabled || !ms->monitor->m_dpmsStatus) {
            Debug::log(TRACE, "Not updating hw cursors: disabled / dpms off display");
            continue;
        }

        if (ms->cursorFrontBuffer) {
            if (ms->monitor->m_output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER)
                ms->monitor->m_output->setCursor(nullptr, {});
            ms->cursorFrontBuffer = nullptr;
        }
    }
}

void CPointerManager::updateCursorBackend() {
    const auto CURSORBOX = getCursorBoxGlobal();

    for (auto const& m : g_pCompositor->m_monitors) {
        if (!m->m_enabled || !m->m_dpmsStatus) {
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

        if (state->softwareLocks > 0 || g_pConfigManager->shouldUseSoftwareCursors(m) || !attemptHardwareCursor(state)) {
            Debug::log(TRACE, "Output {} rejected hardware cursors, falling back to sw", m->m_name);
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

    for (auto const& m : g_pCompositor->m_monitors) {
        auto state = stateFor(m);

        state->box = getCursorBoxLogicalForMonitor(state->monitor.lock());

        auto CROSSES = !m->logicalBox().intersection(CURSORBOX).empty();

        if (!CROSSES && state->cursorFrontBuffer) {
            Debug::log(TRACE, "onCursorMoved for output {}: cursor left the viewport, removing it from the backend", m->m_name);
            setHWCursorBuffer(state, nullptr);
            continue;
        } else if (CROSSES && !state->cursorFrontBuffer) {
            Debug::log(TRACE, "onCursorMoved for output {}: cursor entered the output, but no front buffer, forcing recalc", m->m_name);
            recalc = true;
        }

        if (!state->entered)
            continue;

        CScopeGuard x([m] { m->onCursorMovedOnMonitor(); });

        if (state->hardwareFailed)
            continue;

        const auto CURSORPOS = getCursorPosForMonitor(m);
        m->m_output->moveCursor(CURSORPOS, m->shouldSkipScheduleFrameOnMouseEvent());

        state->monitor->m_scanoutNeedsCursorUpdate = true;
    }

    if (recalc)
        updateCursorBackend();
}

bool CPointerManager::attemptHardwareCursor(SP<CPointerManager::SMonitorPointerState> state) {
    auto output = state->monitor->m_output;

    if (!(output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
        return false;

    const auto CURSORPOS = getCursorPosForMonitor(state->monitor.lock());
    state->monitor->m_output->moveCursor(CURSORPOS, state->monitor->shouldSkipScheduleFrameOnMouseEvent());

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
    if (!(state->monitor->m_output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
        return false;

    const auto HOTSPOT = transformedHotspot(state->monitor.lock());

    Debug::log(TRACE, "[pointer] hw transformed hotspot for {}: {}", state->monitor->m_name, HOTSPOT);

    if (!state->monitor->m_output->setCursor(buf, HOTSPOT))
        return false;

    state->cursorFrontBuffer = buf;

    if (!state->monitor->shouldSkipScheduleFrameOnMouseEvent())
        g_pCompositor->scheduleFrameForMonitor(state->monitor.lock(), Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_SHAPE);

    state->monitor->m_scanoutNeedsCursorUpdate = true;

    return true;
}

SP<Aquamarine::IBuffer> CPointerManager::renderHWCursorBuffer(SP<CPointerManager::SMonitorPointerState> state, SP<CTexture> texture) {
    auto        maxSize    = state->monitor->m_output->cursorPlaneSize();
    auto const& cursorSize = m_currentCursorImage.size;

    static auto PCPUBUFFER = CConfigValue<Hyprlang::INT>("cursor:use_cpu_buffer");

    const bool  shouldUseCpuBuffer = *PCPUBUFFER == 1 || (*PCPUBUFFER != 0 && g_pHyprRenderer->isNvidia());

    if (maxSize == Vector2D{})
        return nullptr;

    if (maxSize != Vector2D{-1, -1}) {
        if (cursorSize.x > maxSize.x || cursorSize.y > maxSize.y) {
            Debug::log(TRACE, "hardware cursor too big! {} > {}", m_currentCursorImage.size, maxSize);
            return nullptr;
        }
    } else
        maxSize = cursorSize;

    if (!state->monitor->m_cursorSwapchain || maxSize != state->monitor->m_cursorSwapchain->currentOptions().size ||
        shouldUseCpuBuffer != (state->monitor->m_cursorSwapchain->getAllocator()->type() != Aquamarine::AQ_ALLOCATOR_TYPE_GBM)) {

        if (!state->monitor->m_cursorSwapchain || shouldUseCpuBuffer != (state->monitor->m_cursorSwapchain->getAllocator()->type() != Aquamarine::AQ_ALLOCATOR_TYPE_GBM)) {

            auto allocator = state->monitor->m_output->getBackend()->preferredAllocator();
            if (shouldUseCpuBuffer) {
                for (const auto& a : state->monitor->m_output->getBackend()->getAllocators()) {
                    if (a->type() == Aquamarine::AQ_ALLOCATOR_TYPE_DRM_DUMB) {
                        allocator = a;
                        break;
                    }
                }
            }

            auto backend                      = state->monitor->m_output->getBackend();
            auto primary                      = backend->getPrimary();
            state->monitor->m_cursorSwapchain = Aquamarine::CSwapchain::create(allocator, primary ? primary.lock() : backend);
        }

        auto options     = state->monitor->m_cursorSwapchain->currentOptions();
        options.size     = maxSize;
        options.length   = 2;
        options.scanout  = true;
        options.cursor   = true;
        options.multigpu = state->monitor->m_output->getBackend()->preferredAllocator()->drmFD() != g_pCompositor->m_drmFD;
        // We do not set the format (unless shm). If it's unset (DRM_FORMAT_INVALID) then the swapchain will pick for us,
        // but if it's set, we don't wanna change it.
        if (shouldUseCpuBuffer)
            options.format = DRM_FORMAT_ARGB8888;

        if (!state->monitor->m_cursorSwapchain->reconfigure(options)) {
            Debug::log(TRACE, "Failed to reconfigure cursor swapchain");
            return nullptr;
        }
    }

    // if we already rendered the cursor, revert the swapchain to avoid rendering the cursor over
    // the current front buffer
    // this flag will be reset in the preRender hook, so when we commit this buffer to KMS
    if (state->cursorRendered)
        state->monitor->m_cursorSwapchain->rollback();

    state->cursorRendered = true;

    auto buf = state->monitor->m_cursorSwapchain->next(nullptr);
    if (!buf) {
        Debug::log(TRACE, "Failed to acquire a buffer from the cursor swapchain");
        return nullptr;
    }

    if (shouldUseCpuBuffer) {
        // get the texture data if available.
        auto texData = texture->dataCopy();
        if (texData.empty()) {
            if (m_currentCursorImage.surface && m_currentCursorImage.surface->resource()->m_role->role() == SURFACE_ROLE_CURSOR) {
                const auto SURFACE   = m_currentCursorImage.surface->resource();
                auto&      shmBuffer = CCursorSurfaceRole::cursorPixelData(SURFACE);

                bool       flipRB = false;

                if (SURFACE->m_current.texture) {
                    Debug::log(TRACE, "Cursor CPU surface: format {}, expecting AR24", NFormatUtils::drmFormatName(SURFACE->m_current.texture->m_drmFormat));
                    if (SURFACE->m_current.texture->m_drmFormat == DRM_FORMAT_ABGR8888) {
                        Debug::log(TRACE, "Cursor CPU surface format AB24, will flip. WARNING: this will break on big endian!");
                        flipRB = true;
                    } else if (SURFACE->m_current.texture->m_drmFormat != DRM_FORMAT_ARGB8888) {
                        Debug::log(TRACE, "Cursor CPU surface format rejected, falling back to sw");
                        return nullptr;
                    }
                }

                if (shmBuffer.data())
                    texData = shmBuffer;
                else {
                    texData.resize(texture->m_size.x * 4 * texture->m_size.y);
                    memset(texData.data(), 0x00, texData.size());
                }

                if (flipRB) {
                    for (size_t i = 0; i < shmBuffer.size(); i += 4) {
                        std::swap(shmBuffer[i], shmBuffer[i + 2]); // little-endian!!!!!!
                    }
                }
            } else {
                Debug::log(TRACE, "Cannot use dumb copy on dmabuf cursor buffers");
                return nullptr;
            }
        }

        // then, we just yeet it into the dumb buffer

        const auto DMABUF      = buf->dmabuf();
        auto [data, fmt, size] = buf->beginDataPtr(0);

        auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, DMABUF.size.x, DMABUF.size.y);
        auto CAIRODATASURFACE =
            cairo_image_surface_create_for_data((unsigned char*)texData.data(), CAIRO_FORMAT_ARGB32, texture->m_size.x, texture->m_size.y, texture->m_size.x * 4);

        auto CAIRO = cairo_create(CAIROSURFACE);

        cairo_set_operator(CAIRO, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(CAIRO, 0, 0, 0, 0);
        cairo_rectangle(CAIRO, 0, 0, texture->m_size.x, texture->m_size.y);
        cairo_fill(CAIRO);

        const auto PATTERNPRE = cairo_pattern_create_for_surface(CAIRODATASURFACE);
        cairo_pattern_set_filter(PATTERNPRE, CAIRO_FILTER_BILINEAR);
        cairo_matrix_t matrixPre;
        cairo_matrix_init_identity(&matrixPre);

        const auto TR = state->monitor->m_transform;

        // we need to scale the cursor to the right size, because it might not be (esp with XCursor)
        const auto SCALE = texture->m_size / (m_currentCursorImage.size / m_currentCursorImage.scale * state->monitor->m_scale);
        cairo_matrix_scale(&matrixPre, SCALE.x, SCALE.y);

        if (TR) {
            cairo_matrix_rotate(&matrixPre, M_PI_2 * (double)TR);

            // FIXME: this is wrong, and doesnt work for 5, 6 and 7. (flipped + rot)
            // cba to do it rn, does anyone fucking use that??
            if (TR >= WL_OUTPUT_TRANSFORM_FLIPPED) {
                cairo_matrix_scale(&matrixPre, -1, 1);
                cairo_matrix_translate(&matrixPre, -DMABUF.size.x, 0);
            }

            if (TR == 3 || TR == 7)
                cairo_matrix_translate(&matrixPre, -DMABUF.size.x, 0);
            else if (TR == 2 || TR == 6)
                cairo_matrix_translate(&matrixPre, -DMABUF.size.x, -DMABUF.size.y);
            else if (TR == 1 || TR == 5)
                cairo_matrix_translate(&matrixPre, 0, -DMABUF.size.y);
        }

        cairo_pattern_set_matrix(PATTERNPRE, &matrixPre);
        cairo_set_source(CAIRO, PATTERNPRE);
        cairo_paint(CAIRO);

        cairo_surface_flush(CAIROSURFACE);

        cairo_pattern_destroy(PATTERNPRE);

        memcpy(data, cairo_image_surface_get_data(CAIROSURFACE), (size_t)cairo_image_surface_get_height(CAIROSURFACE) * cairo_image_surface_get_stride(CAIROSURFACE));

        cairo_destroy(CAIRO);
        cairo_surface_destroy(CAIROSURFACE);
        cairo_surface_destroy(CAIRODATASURFACE);

        buf->endDataPtr();

        return buf;
    }

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_renderData.pMonitor = state->monitor;

    auto RBO = g_pHyprRenderer->getOrCreateRenderbuffer(buf, state->monitor->m_cursorSwapchain->currentOptions().format);
    if (!RBO) {
        Debug::log(TRACE, "Failed to create cursor RB with format {}, mod {}", buf->dmabuf().format, buf->dmabuf().modifier);
        return nullptr;
    }

    RBO->bind();

    g_pHyprOpenGL->beginSimple(state->monitor.lock(), {0, 0, INT16_MAX, INT16_MAX}, RBO);
    g_pHyprOpenGL->clear(CHyprColor{0.F, 0.F, 0.F, 0.F});

    CBox xbox = {{}, Vector2D{m_currentCursorImage.size / m_currentCursorImage.scale * state->monitor->m_scale}.round()};
    Debug::log(TRACE, "[pointer] monitor: {}, size: {}, hw buf: {}, scale: {:.2f}, monscale: {:.2f}, xbox: {}", state->monitor->m_name, m_currentCursorImage.size, cursorSize,
               m_currentCursorImage.scale, state->monitor->m_scale, xbox.size());

    g_pHyprOpenGL->renderTexture(texture, xbox, 1.F);

    g_pHyprOpenGL->end();
    g_pHyprOpenGL->m_renderData.pMonitor.reset();

    g_pHyprRenderer->onRenderbufferDestroy(RBO.get());

    return buf;
}

void CPointerManager::renderSoftwareCursorsFor(PHLMONITOR pMonitor, const Time::steady_tp& now, CRegion& damage, std::optional<Vector2D> overridePos, bool forceRender) {
    if (!hasCursor())
        return;

    auto state = stateFor(pMonitor);

    if (!state->hardwareFailed && state->softwareLocks == 0 && !forceRender) {
        if (m_currentCursorImage.surface)
            m_currentCursorImage.surface->resource()->frame(now);
        return;
    }

    auto box = state->box.copy();
    if (overridePos.has_value()) {
        box.x = overridePos->x;
        box.y = overridePos->y;
    }

    if (box.intersection(CBox{{}, {pMonitor->m_size}}).empty())
        return;

    auto texture = getCurrentCursorTexture();
    if (!texture)
        return;

    box.scale(pMonitor->m_scale);
    box.x = std::round(box.x);
    box.y = std::round(box.y);

    CTexPassElement::SRenderData data;
    data.tex = texture;
    data.box = box.round();

    g_pHyprRenderer->m_renderPass.add(makeShared<CTexPassElement>(data));

    if (m_currentCursorImage.surface)
        m_currentCursorImage.surface->resource()->frame(now);
}

Vector2D CPointerManager::getCursorPosForMonitor(PHLMONITOR pMonitor) {
    return CBox{m_pointerPos - pMonitor->m_position, {0, 0}}
               .transform(wlTransformToHyprutils(invertTransform(pMonitor->m_transform)), pMonitor->m_transformedSize.x / pMonitor->m_scale,
                          pMonitor->m_transformedSize.y / pMonitor->m_scale)
               .pos() *
        pMonitor->m_scale;
}

Vector2D CPointerManager::transformedHotspot(PHLMONITOR pMonitor) {
    if (!pMonitor->m_cursorSwapchain)
        return {}; // doesn't matter, we have no hw cursor, and this is only for hw cursors

    return CBox{m_currentCursorImage.hotspot * pMonitor->m_scale, {0, 0}}
        .transform(wlTransformToHyprutils(invertTransform(pMonitor->m_transform)), pMonitor->m_cursorSwapchain->currentOptions().size.x,
                   pMonitor->m_cursorSwapchain->currentOptions().size.y)
        .pos();
}

CBox CPointerManager::getCursorBoxLogicalForMonitor(PHLMONITOR pMonitor) {
    return getCursorBoxGlobal().translate(-pMonitor->m_position);
}

CBox CPointerManager::getCursorBoxGlobal() {
    return CBox{m_pointerPos, m_currentCursorImage.size / m_currentCursorImage.scale}.translate(-m_currentCursorImage.hotspot);
}

Vector2D CPointerManager::closestValid(const Vector2D& pos) {
    static auto PADDING = CConfigValue<Hyprlang::INT>("cursor:hotspot_padding");

    auto        CURSOR_PADDING = std::clamp((int)*PADDING, 0, 100);
    CBox        hotBox         = {{pos.x - CURSOR_PADDING, pos.y - CURSOR_PADDING}, {2 * CURSOR_PADDING, 2 * CURSOR_PADDING}};

    //
    static auto INSIDE_LAYOUT = [this](const CBox& box) -> bool {
        for (auto const& b : m_currentMonitorLayout.monitorBoxes) {
            if (box.inside(b))
                return true;
        }
        return false;
    };

    static auto INSIDE_LAYOUT_COORD = [this](const Vector2D& vec) -> bool {
        for (auto const& b : m_currentMonitorLayout.monitorBoxes) {
            if (b.containsPoint(vec))
                return true;
        }
        return false;
    };

    static auto NEAREST_LAYOUT = [this](const Vector2D& vec) -> Vector2D {
        Vector2D leader;
        float    distanceSq = __FLT_MAX__;

        for (auto const& b : m_currentMonitorLayout.monitorBoxes) {
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

    for (auto const& mw : m_monitorStates) {
        if (mw->monitor.expired() || !mw->monitor->m_output)
            continue;

        if ((mw->softwareLocks > 0 || mw->hardwareFailed || g_pConfigManager->shouldUseSoftwareCursors(mw->monitor.lock())) &&
            b.overlaps({mw->monitor->m_position, mw->monitor->m_size})) {
            g_pHyprRenderer->damageBox(b, mw->monitor->shouldSkipScheduleFrameOnMouseEvent());
            break;
        }
    }
}

void CPointerManager::warpTo(const Vector2D& logical) {
    damageIfSoftware();

    m_pointerPos = closestValid(logical);

    if (!g_pInputManager->isLocked()) {
        recheckEnteredOutputs();
        onCursorMoved();
    }

    damageIfSoftware();
}

void CPointerManager::move(const Vector2D& deltaLogical) {
    const auto oldPos = m_pointerPos;
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
    const auto& MONITORS = g_pCompositor->m_monitors;
    Vector2D    topLeft = MONITORS.at(0)->m_position, bottomRight = MONITORS.at(0)->m_position + MONITORS.at(0)->m_size;
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
            if (const auto PLASTMONITOR = g_pCompositor->m_lastMonitor.lock(); PLASTMONITOR)
                return PLASTMONITOR->logicalBox();
        } else if (const auto PMONITOR = g_pCompositor->getMonitorFromString(output); PMONITOR)
            return PMONITOR->logicalBox();
        return mappedArea;
    };

    switch (dev->getType()) {
        case HID_TYPE_TABLET: {
            CTablet* TAB = reinterpret_cast<CTablet*>(dev.get());
            if (!TAB->m_boundOutput.empty()) {
                mappedArea = outputMappedArea(TAB->m_boundOutput);
                mappedArea.translate(TAB->m_boundBox.pos());
            } else if (TAB->m_absolutePos) {
                mappedArea.x = TAB->m_boundBox.x;
                mappedArea.y = TAB->m_boundBox.y;
            } else
                mappedArea.translate(TAB->m_boundBox.pos());

            if (!TAB->m_boundBox.empty()) {
                mappedArea.w = TAB->m_boundBox.w;
                mappedArea.h = TAB->m_boundBox.h;
            }
            break;
        }
        case HID_TYPE_TOUCH: {
            ITouch* TOUCH = reinterpret_cast<ITouch*>(dev.get());
            if (!TOUCH->m_boundOutput.empty())
                mappedArea = outputMappedArea(TOUCH->m_boundOutput);
            break;
        }
        case HID_TYPE_POINTER: {
            IPointer* POINTER = reinterpret_cast<IPointer*>(dev.get());
            if (!POINTER->m_boundOutput.empty())
                mappedArea = outputMappedArea(POINTER->m_boundOutput);
            break;
        }
        default: break;
    }

    damageIfSoftware();

    if (std::isnan(abs.x) || std::isnan(abs.y)) {
        m_pointerPos.x = std::isnan(abs.x) ? m_pointerPos.x : mappedArea.x + mappedArea.w * abs.x;
        m_pointerPos.y = std::isnan(abs.y) ? m_pointerPos.y : mappedArea.y + mappedArea.h * abs.y;
    } else
        m_pointerPos = mappedArea.pos() + mappedArea.size() * abs;

    onCursorMoved();
    recheckEnteredOutputs();

    damageIfSoftware();
}

void CPointerManager::onMonitorLayoutChange() {
    m_currentMonitorLayout.monitorBoxes.clear();
    for (auto const& m : g_pCompositor->m_monitors) {
        if (m->isMirror() || !m->m_enabled || !m->m_output)
            continue;

        m_currentMonitorLayout.monitorBoxes.emplace_back(m->m_position, m->m_size);
    }

    damageIfSoftware();

    m_pointerPos = closestValid(m_pointerPos);
    updateCursorBackend();
    recheckEnteredOutputs();

    damageIfSoftware();
}

SP<CTexture> CPointerManager::getCurrentCursorTexture() {
    if (!m_currentCursorImage.pBuffer && (!m_currentCursorImage.surface || !m_currentCursorImage.surface->resource()->m_current.texture))
        return nullptr;

    if (m_currentCursorImage.pBuffer) {
        if (!m_currentCursorImage.bufferTex)
            m_currentCursorImage.bufferTex = makeShared<CTexture>(m_currentCursorImage.pBuffer, true);
        return m_currentCursorImage.bufferTex;
    }

    return m_currentCursorImage.surface->resource()->m_current.texture;
}

void CPointerManager::attachPointer(SP<IPointer> pointer) {
    if (!pointer)
        return;

    static auto PMOUSEDPMS = CConfigValue<Hyprlang::INT>("misc:mouse_move_enables_dpms");

    //
    auto listener = m_pointerListeners.emplace_back(makeShared<SPointerListener>());

    listener->pointer = pointer;

    // clang-format off
    listener->destroy = pointer->m_events.destroy.registerListener([this] (std::any d) {
        detachPointer(nullptr);
    });

    listener->motion = pointer->m_pointerEvents.motion.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SMotionEvent>(e);

        g_pInputManager->onMouseMoved(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_dpmsStateOn && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->motionAbsolute = pointer->m_pointerEvents.motionAbsolute.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SMotionAbsoluteEvent>(e);

        g_pInputManager->onMouseWarp(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_dpmsStateOn && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->button = pointer->m_pointerEvents.button.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SButtonEvent>(e);

        g_pInputManager->onMouseButton(E);

        PROTO::idle->onActivity();
    });

    listener->axis = pointer->m_pointerEvents.axis.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SAxisEvent>(e);

        g_pInputManager->onMouseWheel(E);

        PROTO::idle->onActivity();
    });

    listener->frame = pointer->m_pointerEvents.frame.registerListener([] (std::any e) {
        bool shouldSkip = false;
        if (!g_pSeatManager->m_mouse.expired() && g_pInputManager->isLocked()) {
            auto PMONITOR = g_pCompositor->m_lastMonitor.get();
            shouldSkip = PMONITOR && PMONITOR->shouldSkipScheduleFrameOnMouseEvent();
        }
        g_pSeatManager->m_isPointerFrameSkipped = shouldSkip;
        if (!g_pSeatManager->m_isPointerFrameSkipped)
            g_pSeatManager->sendPointerFrame();
    });

    listener->swipeBegin = pointer->m_pointerEvents.swipeBegin.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeBeginEvent>(e);

        g_pInputManager->onSwipeBegin(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_dpmsStateOn && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->swipeEnd = pointer->m_pointerEvents.swipeEnd.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeEndEvent>(e);

        g_pInputManager->onSwipeEnd(E);

        PROTO::idle->onActivity();
    });

    listener->swipeUpdate = pointer->m_pointerEvents.swipeUpdate.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SSwipeUpdateEvent>(e);

        g_pInputManager->onSwipeUpdate(E);

        PROTO::idle->onActivity();
    });

    listener->pinchBegin = pointer->m_pointerEvents.pinchBegin.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchBeginEvent>(e);

        PROTO::pointerGestures->pinchBegin(E.timeMs, E.fingers);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_dpmsStateOn && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->pinchEnd = pointer->m_pointerEvents.pinchEnd.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchEndEvent>(e);

        PROTO::pointerGestures->pinchEnd(E.timeMs, E.cancelled);

        PROTO::idle->onActivity();
    });

    listener->pinchUpdate = pointer->m_pointerEvents.pinchUpdate.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SPinchUpdateEvent>(e);

        PROTO::pointerGestures->pinchUpdate(E.timeMs, E.delta, E.scale, E.rotation);

        PROTO::idle->onActivity();
    });

    listener->holdBegin = pointer->m_pointerEvents.holdBegin.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SHoldBeginEvent>(e);

        PROTO::pointerGestures->holdBegin(E.timeMs, E.fingers);

        PROTO::idle->onActivity();
    });

    listener->holdEnd = pointer->m_pointerEvents.holdEnd.registerListener([] (std::any e) {
        auto E = std::any_cast<IPointer::SHoldEndEvent>(e);

        PROTO::pointerGestures->holdEnd(E.timeMs, E.cancelled);

        PROTO::idle->onActivity();
    });
    // clang-format on

    Debug::log(LOG, "Attached pointer {} to global", pointer->m_hlName);
}

void CPointerManager::attachTouch(SP<ITouch> touch) {
    if (!touch)
        return;

    static auto PMOUSEDPMS = CConfigValue<Hyprlang::INT>("misc:mouse_move_enables_dpms");

    //
    auto listener = m_touchListeners.emplace_back(makeShared<STouchListener>());

    listener->touch = touch;

    // clang-format off
    listener->destroy = touch->m_events.destroy.registerListener([this] (std::any d) {
        detachTouch(nullptr);
    });

    listener->down = touch->m_touchEvents.down.registerListener([] (std::any e) {
        auto E = std::any_cast<ITouch::SDownEvent>(e);

        g_pInputManager->onTouchDown(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_dpmsStateOn && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->up = touch->m_touchEvents.up.registerListener([] (std::any e) {
        auto E = std::any_cast<ITouch::SUpEvent>(e);

        g_pInputManager->onTouchUp(E);

        PROTO::idle->onActivity();
    });

    listener->motion = touch->m_touchEvents.motion.registerListener([] (std::any e) {
        auto E = std::any_cast<ITouch::SMotionEvent>(e);

        g_pInputManager->onTouchMove(E);

        PROTO::idle->onActivity();
    });

    listener->cancel = touch->m_touchEvents.cancel.registerListener([] (std::any e) {
        //
    });

    listener->frame = touch->m_touchEvents.frame.registerListener([] (std::any e) {
        g_pSeatManager->sendTouchFrame();
    });
    // clang-format on

    Debug::log(LOG, "Attached touch {} to global", touch->m_hlName);
}

void CPointerManager::attachTablet(SP<CTablet> tablet) {
    if (!tablet)
        return;

    static auto PMOUSEDPMS = CConfigValue<Hyprlang::INT>("misc:mouse_move_enables_dpms");

    //
    auto listener = m_tabletListeners.emplace_back(makeShared<STabletListener>());

    listener->tablet = tablet;

    // clang-format off
    listener->destroy = tablet->m_events.destroy.registerListener([this] (std::any d) {
        detachTablet(nullptr);
    });

    listener->axis = tablet->m_tabletEvents.axis.registerListener([] (std::any e) {
        auto E = std::any_cast<CTablet::SAxisEvent>(e);

        g_pInputManager->onTabletAxis(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_dpmsStateOn && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->proximity = tablet->m_tabletEvents.proximity.registerListener([] (std::any e) {
        auto E = std::any_cast<CTablet::SProximityEvent>(e);

        g_pInputManager->onTabletProximity(E);

        PROTO::idle->onActivity();
    });

    listener->tip = tablet->m_tabletEvents.tip.registerListener([] (std::any e) {
        auto E = std::any_cast<CTablet::STipEvent>(e);

        g_pInputManager->onTabletTip(E);

        PROTO::idle->onActivity();

        if (!g_pCompositor->m_dpmsStateOn && *PMOUSEDPMS)
            g_pKeybindManager->dpms("on");
    });

    listener->button = tablet->m_tabletEvents.button.registerListener([] (std::any e) {
        auto E = std::any_cast<CTablet::SButtonEvent>(e);

        g_pInputManager->onTabletButton(E);

        PROTO::idle->onActivity();
    });
    // clang-format on

    Debug::log(LOG, "Attached tablet {} to global", tablet->m_hlName);
}

void CPointerManager::detachPointer(SP<IPointer> pointer) {
    std::erase_if(m_pointerListeners, [pointer](const auto& e) { return e->pointer.expired() || e->pointer == pointer; });
}

void CPointerManager::detachTouch(SP<ITouch> touch) {
    std::erase_if(m_touchListeners, [touch](const auto& e) { return e->touch.expired() || e->touch == touch; });
}

void CPointerManager::detachTablet(SP<CTablet> tablet) {
    std::erase_if(m_tabletListeners, [tablet](const auto& e) { return e->tablet.expired() || e->tablet == tablet; });
}

void CPointerManager::damageCursor(PHLMONITOR pMonitor) {
    for (auto const& mw : m_monitorStates) {
        if (mw->monitor != pMonitor)
            continue;

        auto b = getCursorBoxGlobal().intersection(pMonitor->logicalBox());

        if (b.empty())
            return;

        g_pHyprRenderer->damageBox(b);

        return;
    }
}

Vector2D CPointerManager::cursorSizeLogical() {
    return m_currentCursorImage.size / m_currentCursorImage.scale;
}

void CPointerManager::storeMovement(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel) {
    m_storedTime = time;
    m_storedDelta += delta;
    m_storedUnaccel += deltaUnaccel;
}

void CPointerManager::setStoredMovement(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel) {
    m_storedTime    = time;
    m_storedDelta   = delta;
    m_storedUnaccel = deltaUnaccel;
}

void CPointerManager::sendStoredMovement() {
    PROTO::relativePointer->sendRelativeMotion((uint64_t)m_storedTime * 1000, m_storedDelta, m_storedUnaccel);
    m_storedTime    = 0;
    m_storedDelta   = Vector2D{};
    m_storedUnaccel = Vector2D{};
}
