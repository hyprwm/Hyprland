#include "Water.hpp"

#include "Kawase.hpp"

#include "../GLFramebuffer.hpp"
#include "../../OpenGL.hpp"
#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../desktop/state/ViewState.hpp"
#include "../../../desktop/view/window/Window.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../managers/input/InputManager.hpp"
#include "../../../pointer/PointerManager.hpp"
#include "../../../state/MonitorState.hpp"

#include <algorithm>
#include <cmath>
#include <drm_fourcc.h>
#include <ranges>

using namespace Render;
using namespace Render::GL;

static constexpr float  MAX_WATER_DISPLACEMENT = 32.F;
static constexpr float  SIMULATION_SCALE       = 0.25F;
static constexpr float  MIN_SIMULATION_SIZE    = 32.F;
static constexpr float  MAX_SIMULATION_SIZE    = 512.F;
static constexpr float  WATER_FADE_DURATION    = 2.F;
static constexpr size_t MAX_STORED_IMPULSES    = 64;

CWaterBlurMaterial::CWaterBlurMaterial(CHyprOpenGLImpl& impl) : m_impl(impl) {
    m_listeners.mouseButton = Event::bus()->m_events.input.mouse.button.listen([this](IPointer::SButtonEvent event, Event::SCallbackInfo&) {
        m_lastMouseHeldCoord.reset();

        if (event.state == WL_POINTER_BUTTON_STATE_PRESSED) {
            m_mouseHeld = true;
            addImpulse();
            return;
        }

        if (event.state == WL_POINTER_BUTTON_STATE_RELEASED)
            m_mouseHeld = false;
    });

    m_listeners.mouseMotion = Event::bus()->m_events.input.mouse.move.listen([this](Vector2D position, Event::SCallbackInfo&) {
        if (!m_mouseHeld)
            return;

        if (m_lastMouseHeldCoord && (*m_lastMouseHeldCoord - position).size() < 6.9F)
            return;

        addImpulse();
        m_lastMouseHeldCoord = position;
    });

    m_listeners.renderPre = Event::bus()->m_events.render.pre.listen([this](PHLMONITOR) { ++m_frame; });
    m_listeners.windowDestroy =
        Event::bus()->m_events.window.destroy.listen([this](PHLWINDOWREF window) { std::erase_if(m_windowStates, [&](const auto& state) { return state.window == window; }); });
    m_listeners.config = Event::bus()->m_events.config.props_refreshed.listen([this](const bool) {
        for (auto& state : m_windowStates)
            state.reset = true;
        for (auto& state : m_monitorStates)
            state.reset = true;
    });
}

CWaterBlurProvider::CWaterBlurProvider(CHyprOpenGLImpl& impl) : CDualKawaseBlurProvider(impl, makeUnique<CWaterBlurMaterial>(impl)) {
    ;
}

eBlurType CWaterBlurMaterial::type() const noexcept {
    return eBlurType::BLUR_WATER;
}

bool CWaterBlurMaterial::isAnimated(const CRenderingContext& context) const noexcept {
    static auto PBLURENABLED   = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    static auto PWATERSTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:water:strength");

    if (!*PBLURENABLED || *PWATERSTRENGTH <= 0.F)
        return false;

    pruneStates();

    const auto monitor = context.sceneMonitor;
    if (!monitor)
        return false;

    const auto now = Time::steadyNow();
    return std::ranges::any_of(m_windowStates, [&](const auto& state) { return state.monitor == monitor && stateIsActive(state, now); }) ||
        std::ranges::any_of(m_monitorStates, [&](const auto& state) { return state.monitor == monitor && stateIsActive(state, now); });
}

SBlurMaterialRequirements CWaterBlurMaterial::requirements() const noexcept {
    return {
        .finishFragment = SH_FRAG_WATERFINISH,
    };
}

void CWaterBlurMaterial::prepare(const SBlurMaterialContext& context) {
    pruneStates();

    const auto state = stateForContext(context.renderingContext, context.blurContext, false);
    if (!state || !context.renderingContext.sceneMonitor)
        return;

    const auto extent = transformedPatternBox(context.renderingContext, context.blurContext);
    state->monitor    = context.renderingContext.sceneMonitor;
    updateState(context.renderingContext, *state, extent);
}

void CWaterBlurMaterial::bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const {
    static auto PWATERSTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:water:strength");

    const auto  state = stateForContext(context.renderingContext, context.blurContext);
    if (!state || !state->buffers[state->currentBuffer]) {
        shader->setUniformInt(SHADER_WATER_ENABLED, 0);
        return;
    }

    const auto extent = transformedPatternBox(context.renderingContext, context.blurContext);
    if (extent.width <= 0 || extent.height <= 0) {
        shader->setUniformInt(SHADER_WATER_ENABLED, 0);
        return;
    }

    glActiveTexture(GL_TEXTURE2);
    const auto texture = state->buffers[state->currentBuffer]->getTexture();
    texture->bind();
    texture->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    texture->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glActiveTexture(GL_TEXTURE0);

    shader->setUniformInt(SHADER_WATER_ENABLED, 1);
    shader->setUniformInt(SHADER_WATER_STATE_TEX, 2);
    shader->setUniformFloat2(SHADER_WATER_TEXEL_SIZE, 1.F / state->simulationSize.x, 1.F / state->simulationSize.y);
    shader->setUniformFloat4(SHADER_WATER_EXTENT, sc<float>(extent.x), sc<float>(extent.y), sc<float>(extent.width), sc<float>(extent.height));
    const auto secondsRemaining = std::chrono::duration<float>(state->activeUntil - Time::steadyNow()).count();
    const auto fade             = std::clamp(secondsRemaining / WATER_FADE_DURATION, 0.F, 1.F);
    shader->setUniformFloat(SHADER_WATER_REFRACTION, std::clamp(*PWATERSTRENGTH, 0.F, MAX_WATER_DISPLACEMENT) * std::clamp(context.strength, 0.F, 1.F) * fade);
}

float CWaterBlurMaterial::sampleRadius() const {
    static auto PWATERSTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:water:strength");

    return std::ceil(std::clamp(*PWATERSTRENGTH, 0.F, MAX_WATER_DISPLACEMENT));
}

CWaterBlurMaterial::SState* CWaterBlurMaterial::stateForContext(const CRenderingContext& renderingContext, const SBlurContext& context, bool create) {
    if (!context.owner.expired())
        return windowState(context.owner, create);

    return monitorState(renderingContext.sceneMonitor, create);
}

const CWaterBlurMaterial::SState* CWaterBlurMaterial::stateForContext(const CRenderingContext& renderingContext, const SBlurContext& context) const {
    if (!context.owner.expired()) {
        const auto state = std::ranges::find_if(m_windowStates, [&](const auto& candidate) { return candidate.window == context.owner; });
        return state != m_windowStates.end() ? &*state : nullptr;
    }

    const auto monitor = renderingContext.sceneMonitor;
    const auto state   = std::ranges::find_if(m_monitorStates, [&](const auto& candidate) { return candidate.monitor == monitor; });
    return state != m_monitorStates.end() ? &*state : nullptr;
}

CWaterBlurMaterial::SState* CWaterBlurMaterial::windowState(PHLWINDOWREF window, bool create) {
    const auto state = std::ranges::find_if(m_windowStates, [&](const auto& candidate) { return candidate.window == window; });
    if (state != m_windowStates.end())
        return &*state;

    if (!create)
        return nullptr;

    return &m_windowStates.emplace_back(SState{.window = window});
}

CWaterBlurMaterial::SState* CWaterBlurMaterial::monitorState(PHLMONITORREF monitor, bool create) {
    const auto state = std::ranges::find_if(m_monitorStates, [&](const auto& candidate) { return candidate.monitor == monitor; });
    if (state != m_monitorStates.end())
        return &*state;

    if (!create)
        return nullptr;

    return &m_monitorStates.emplace_back(SState{.monitor = monitor});
}

void CWaterBlurMaterial::addImpulse() {
    static auto PBLURENABLED   = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    static auto PWATERSTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:water:strength");
    static auto PWATERRADIUS   = CConfigValue<Config::FLOAT>("decoration:blur:water:radius");

    if (!*PBLURENABLED || *PWATERSTRENGTH <= 0.F)
        return;

    const auto position = g_pInputManager->getMouseCoordsInternal();
    const auto monitor  = State::monitorState()->query().vec(position).run();
    if (!monitor)
        return;

    const auto localPosition   = (position - monitor->m_position) * monitor->m_scale;
    const auto monitorPosition = Vector2D{localPosition.x / monitor->m_transformedSize.x, localPosition.y / monitor->m_transformedSize.y};
    const auto amplitude       = std::clamp(*PWATERSTRENGTH / MAX_WATER_DISPLACEMENT, 0.F, 1.F) * 0.5F;

    queueImpulse(*monitorState(monitor, true), monitorPosition, *PWATERRADIUS, amplitude);

    static auto PBLURSIZE   = CConfigValue<Config::INTEGER>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Config::INTEGER>("decoration:blur:passes");

    const auto  reach      = *PWATERRADIUS + waterDamageRadius(*PBLURSIZE, *PBLURPASSES, *PWATERSTRENGTH);
    monitor->m_blurFBDirty = true;
    monitor->addDamage(CBox{
        std::floor(localPosition.x - reach),
        std::floor(localPosition.y - reach),
        std::ceil(reach * 2.F),
        std::ceil(reach * 2.F),
    });

    const auto window = Desktop::viewState()->hitTest().windowAt(position, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);
    if (!window)
        return;

    const auto box = window->logicalBox();
    if (!box || box->width <= 0 || box->height <= 0)
        return;

    const auto windowPosition = Vector2D{(position.x - box->x) / box->width, (position.y - box->y) / box->height};
    const auto state          = windowState(window, true);
    state->monitor            = monitor;
    queueImpulse(*state, windowPosition, *PWATERRADIUS, amplitude);
}

void CWaterBlurMaterial::queueImpulse(SState& state, Vector2D position, float radius, float amplitude) {
    static auto PWATERDURATION = CConfigValue<Config::FLOAT>("decoration:blur:water:duration");

    position.x = std::clamp(position.x, 0.0, 1.0);
    position.y = std::clamp(position.y, 0.0, 1.0);

    state.impulses.push_back({.position = position, .radius = std::max(radius, 1.F), .amplitude = amplitude});
    if (state.impulses.size() > MAX_STORED_IMPULSES)
        state.impulses.erase(state.impulses.begin());

    const auto duration = std::clamp(*PWATERDURATION, 0.5F, 60.F);
    state.activeUntil   = Time::steadyNow() + std::chrono::duration_cast<Time::steady_dur>(std::chrono::duration<float>(duration));
}

void CWaterBlurMaterial::updateState(CRenderingContext& renderingContext, SState& state, const CBox& extent) {
    const auto now = Time::steadyNow();
    if (!stateIsActive(state, now) || state.lastFrame == m_frame)
        return;

    const auto simulationSize = Vector2D{
        std::clamp(std::ceil(extent.width * SIMULATION_SCALE), sc<double>(MIN_SIMULATION_SIZE), sc<double>(MAX_SIMULATION_SIZE)),
        std::clamp(std::ceil(extent.height * SIMULATION_SCALE), sc<double>(MIN_SIMULATION_SIZE), sc<double>(MAX_SIMULATION_SIZE)),
    };

    if (state.reset || state.simulationSize != simulationSize)
        resetState(renderingContext, state, simulationSize);

    const auto dt = state.lastUpdate == Time::steady_tp{} ? 1.F / 60.F : std::clamp(sc<float>(std::chrono::duration<float>(now - state.lastUpdate).count()), 0.F, 1.F / 20.F);
    drawStateStep(renderingContext, state, dt, extent);
    state.lastUpdate = now;
    state.lastFrame  = m_frame;
}

void CWaterBlurMaterial::resetState(CRenderingContext& renderingContext, SState& state, const Vector2D& simulationSize) {
    for (auto& buffer : state.buffers) {
        if (!buffer)
            buffer = dynamicPointerCast<CGLFramebuffer>(g_pHyprRenderer->createFB("Water simulation"));

        buffer->alloc(sc<int>(simulationSize.x), sc<int>(simulationSize.y), DRM_FORMAT_ARGB8888);
        buffer->bind();
        g_pHyprRenderer->disableScissor(renderingContext);
        glClearColor(0.5F, 0.5F, 0.F, 1.F);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    state.simulationSize = simulationSize;
    state.currentBuffer  = 0;
    state.lastUpdate     = {};
    state.reset          = false;
}

void CWaterBlurMaterial::drawStateStep(CRenderingContext& renderingContext, SState& state, float dt, const CBox& extent) {
    static auto PWATERSPEED   = CConfigValue<Config::FLOAT>("decoration:blur:water:speed");
    static auto PWATERDAMPING = CConfigValue<Config::FLOAT>("decoration:blur:water:damping");

    const auto  source = state.buffers[state.currentBuffer];
    const auto  target = state.buffers[1 - state.currentBuffer];
    target->bind();
    g_pHyprRenderer->setViewport(0, 0, sc<int>(state.simulationSize.x), sc<int>(state.simulationSize.y));
    g_pHyprRenderer->disableScissor(renderingContext);

    glActiveTexture(GL_TEXTURE0);
    const auto texture = source->getTexture();
    texture->bind();
    texture->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    texture->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    const auto monitor = renderingContext.sceneMonitor;
    const auto matrix  = g_pHyprRenderer->projectBoxToTarget(renderingContext, {0, 0, monitor->m_transformedSize.x, monitor->m_transformedSize.y});
    const auto shader  = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_WATERSTEP));
    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, matrix.getMatrix());
    shader->setUniformInt(SHADER_WATER_STATE_TEX, 0);
    shader->setUniformFloat2(SHADER_WATER_TEXEL_SIZE, 1.F / state.simulationSize.x, 1.F / state.simulationSize.y);
    shader->setUniformFloat4(SHADER_WATER_PARAMS, dt, std::clamp(*PWATERSPEED, 0.F, 10.F), std::clamp(*PWATERDAMPING, 0.F, 1.F), 0.F);

    std::vector<float> impulses;
    impulses.reserve(std::min(state.impulses.size(), MAX_IMPULSES) * 4);
    const auto scale = std::max(std::min(sc<float>(extent.width), sc<float>(extent.height)), 1.F);
    for (const auto& impulse : state.impulses | std::views::take(MAX_IMPULSES))
        impulses.insert(impulses.end(), {sc<float>(impulse.position.x), sc<float>(impulse.position.y), impulse.radius / scale, impulse.amplitude});

    shader->setUniformInt(SHADER_WATER_IMPULSE_COUNT, sc<int>(impulses.size() / 4));
    if (!impulses.empty())
        shader->setUniform4fv(SHADER_WATER_IMPULSES, sc<GLsizei>(impulses.size() / 4), impulses);

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    state.impulses.clear();
    state.currentBuffer = 1 - state.currentBuffer;
}

CBox CWaterBlurMaterial::transformedPatternBox(const CRenderingContext& renderingContext, const SBlurContext& context) const {
    const auto monitor = renderingContext.sceneMonitor;
    if (!monitor)
        return {};

    return context.patternBox.value_or(CBox{0, 0, monitor->m_transformedSize.x, monitor->m_transformedSize.y});
}

bool CWaterBlurMaterial::stateIsActive(const SState& state, const Time::steady_tp& now) const {
    return !state.impulses.empty() || (state.activeUntil != Time::steady_tp{} && now < state.activeUntil);
}

void CWaterBlurMaterial::pruneStates() const {
    const auto now = Time::steadyNow();
    std::erase_if(m_windowStates, [&](const auto& state) { return state.window.expired() || !state.window->shouldBlur() || !stateIsActive(state, now); });
    std::erase_if(m_monitorStates, [&](const auto& state) { return state.monitor.expired() || !stateIsActive(state, now); });
}

float Render::GL::waterDamageRadius(int64_t size, int64_t passes, float displacement) {
    return dualKawaseDamageRadius(size, passes) + std::ceil(std::clamp(displacement, 0.F, MAX_WATER_DISPLACEMENT));
}
