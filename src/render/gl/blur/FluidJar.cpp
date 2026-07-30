#include "FluidJar.hpp"

#include "../GLFramebuffer.hpp"
#include "../../OpenGL.hpp"
#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../desktop/view/Window.hpp"
#include "../../../desktop/Workspace.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../helpers/Color.hpp"
#include "../../../helpers/cm/ColorManagement.hpp"

#include <algorithm>
#include <cmath>
#include <drm_fourcc.h>
#include <numbers>
#include <ranges>

using namespace Render;
using namespace Render::GL;
using namespace NColorManagement;

static constexpr float  BASE_SIMULATION_SCALE = 0.25F;
static constexpr int    BASE_SIMULATION_SIDE  = 256;
static constexpr float  MIN_PRECISION         = 0.5F;
static constexpr float  MAX_PRECISION         = 8.F;
static constexpr int    MAX_PARTICLES         = 131072;
static constexpr float  FIXED_TIMESTEP        = 1.F / 60.F;
static constexpr float  SOLVER_TIMESTEP       = 3.F;
static constexpr float  MAX_WALL_SPEED        = 1.25F;
static constexpr float  MAX_MOTION_INTERVAL   = 0.1F;
static constexpr int    MAX_SUBSTEPS          = 8;
static constexpr int    INITIAL_GRAPH_STEPS   = 8;
static constexpr int    INITIAL_TRACK_STEPS   = 4;
static constexpr int    INITIAL_VISUAL_STEPS  = 4;
static constexpr float  MAX_REFRACTION        = 8.F;
static constexpr float  MAX_TURBULENCE        = 5.F;
static constexpr float  MAX_DISTORTION        = 10.F;
static constexpr double ANIMATION_PERIOD      = 200.0 * std::numbers::pi;

static void             bindNearestTexture(SP<CGLFramebuffer> buffer, GLenum unit) {
    glActiveTexture(unit);
    const auto texture = buffer->getTexture();
    texture->bind();
    texture->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    texture->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static bool geometryChanged(const CBox& lhs, const CBox& rhs) {
    return lhs.x != rhs.x || lhs.y != rhs.y || lhs.width != rhs.width || lhs.height != rhs.height;
}

static bool geometryDiscontinuous(const CBox& oldExtent, const CBox& newExtent, float elapsed) {
    if (elapsed <= 0.F || elapsed > MAX_MOTION_INTERVAL)
        return true;

    const auto oldRight        = oldExtent.x + oldExtent.width;
    const auto newRight        = newExtent.x + newExtent.width;
    const auto oldBottom       = oldExtent.y + oldExtent.height;
    const auto newBottom       = newExtent.y + newExtent.height;
    const auto horizontalDelta = std::max(std::abs(newExtent.x - oldExtent.x), std::abs(newRight - oldRight));
    const auto verticalDelta   = std::max(std::abs(newExtent.y - oldExtent.y), std::abs(newBottom - oldBottom));
    const auto width           = std::max({oldExtent.width, newExtent.width, 1.0});
    const auto height          = std::max({oldExtent.height, newExtent.height, 1.0});
    return horizontalDelta > width * 0.75 || verticalDelta > height * 0.75;
}

static CBox renderedWindowBox(PHLWINDOW window) {
    if (!window)
        return {};

    auto position = window->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT) + window->m_floatingOffset;
    if (!window->m_pinned && window->m_workspace)
        position += window->m_workspace->m_renderOffset->value();

    const auto size = window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    return {position.x, position.y, size.x, size.y};
}

CFluidJarBlurProvider::CFluidJarBlurProvider(CHyprOpenGLImpl& impl) : CDualKawaseBlurProvider(impl), m_impl(impl), m_supported(impl.m_exts.EXT_color_buffer_half_float) {
    if (!m_supported)
        Log::logger->log(Log::WARN, "fluid_jar blur requires GL_EXT_color_buffer_half_float; falling back to Kawase blur");

    m_listeners.renderPre = Event::bus()->m_events.render.pre.listen([this](PHLMONITOR) { ++m_frame; });
    m_listeners.windowDestroy =
        Event::bus()->m_events.window.destroy.listen([this](PHLWINDOWREF window) { std::erase_if(m_states, [&](const auto& state) { return state.window == window; }); });
}

eBlurType CFluidJarBlurProvider::type() const noexcept {
    return eBlurType::BLUR_FLUID_JAR;
}

bool CFluidJarBlurProvider::isAnimated() const noexcept {
    return false;
}

bool CFluidJarBlurProvider::requiresLiveBlur() const noexcept {
    return m_supported;
}

ePreparedFragmentShader CFluidJarBlurProvider::finishFragment() const noexcept {
    return SH_FRAG_FLUIDJARFINISH;
}

bool CFluidJarBlurProvider::requiresPreparedInput() const noexcept {
    return m_supported;
}

void CFluidJarBlurProvider::updateProviderState(const SBlurContext& context, const CRegion& outputDamage) {
    if (!m_supported || context.owner.expired())
        return;

    pruneStates();

    const auto state         = stateForContext(context, true);
    const auto renderExtent  = transformedPatternBox(context);
    const auto window        = context.owner.lock();
    const auto physicsExtent = renderedWindowBox(window);
    if (!state || physicsExtent.width <= 0 || physicsExtent.height <= 0 || renderExtent.width <= 0 || renderExtent.height <= 0 || !g_pHyprRenderer->m_renderData.pMonitor)
        return;

    updateState(*state, physicsExtent);
}

void CFluidJarBlurProvider::setFinishUniforms(WP<CShader> shader, float strength, const SBlurContext& context) const {
    const auto state = stateForContext(context);
    if (!m_supported || !state || !state->visual[state->currentVisual]) {
        shader->setUniformInt(SHADER_FLUIDJAR_ENABLED, 0);
        return;
    }

    const auto extent = transformedPatternBox(context);
    if (extent.width <= 0 || extent.height <= 0) {
        shader->setUniformInt(SHADER_FLUIDJAR_ENABLED, 0);
        return;
    }

    glActiveTexture(GL_TEXTURE2);
    const auto texture = state->visual[state->currentVisual]->getTexture();
    texture->bind();
    texture->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    texture->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glActiveTexture(GL_TEXTURE0);

    static auto PFLUIDCOLOR      = CConfigValue<Config::INTEGER>("decoration:blur:fluid_jar:color");
    static auto PFLUIDTURBULENCE = CConfigValue<Config::FLOAT>("decoration:blur:fluid_jar:turbulence");
    static auto PFLUIDDISTORTION = CConfigValue<Config::FLOAT>("decoration:blur:fluid_jar:distortion");
    const auto  color            = CHyprColor(*PFLUIDCOLOR);
    const auto  animationPhase   = sc<float>(std::fmod(state->animationTime, ANIMATION_PERIOD));

    shader->setUniformInt(SHADER_FLUIDJAR_ENABLED, 1);
    shader->setUniformInt(SHADER_FLUIDJAR_VISUAL_TEX, 2);
    shader->setUniformFloat4(SHADER_FLUIDJAR_EXTENT, sc<float>(extent.x), sc<float>(extent.y), sc<float>(extent.width), sc<float>(extent.height));
    shader->setUniformFloat4(SHADER_FLUIDJAR_COLOR, color.r, color.g, color.b, color.a);
    shader->setUniformFloat(SHADER_FLUIDJAR_REFRACTION, MAX_REFRACTION);
    shader->setUniformInt(SHADER_FLUIDJAR_TRANSFER_FUNCTION, sc<int>(getDefaultImageDescription()->value().transferFunction));
    shader->setUniformFloat(SHADER_FLUIDJAR_STRENGTH, std::clamp(strength, 0.F, 1.F));
    shader->setUniformFloat(SHADER_FLUIDJAR_TURBULENCE, std::clamp(*PFLUIDTURBULENCE, 0.F, MAX_TURBULENCE));
    shader->setUniformFloat(SHADER_FLUIDJAR_DISTORTION, std::clamp(*PFLUIDDISTORTION, 0.F, MAX_DISTORTION));
    shader->setUniformFloat(SHADER_TIME, animationPhase);
}

float CFluidJarBlurProvider::damageRadius() const {
    if (!m_supported)
        return CDualKawaseBlurProvider::damageRadius();

    static auto PBLURSIZE   = CConfigValue<Config::INTEGER>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Config::INTEGER>("decoration:blur:passes");
    static auto PDISTORTION = CConfigValue<Config::FLOAT>("decoration:blur:fluid_jar:distortion");
    return fluidJarDamageRadius(*PBLURSIZE, *PBLURPASSES, *PDISTORTION);
}

CFluidJarBlurProvider::SState* CFluidJarBlurProvider::stateForContext(const SBlurContext& context, bool create) {
    if (context.owner.expired())
        return nullptr;

    const auto state = std::ranges::find_if(m_states, [&](const auto& candidate) { return candidate.window == context.owner; });
    if (state != m_states.end())
        return &*state;

    if (!create)
        return nullptr;

    return &m_states.emplace_back(SState{.window = context.owner});
}

const CFluidJarBlurProvider::SState* CFluidJarBlurProvider::stateForContext(const SBlurContext& context) const {
    if (context.owner.expired())
        return nullptr;

    const auto state = std::ranges::find_if(m_states, [&](const auto& candidate) { return candidate.window == context.owner; });
    return state != m_states.end() ? &*state : nullptr;
}

void CFluidJarBlurProvider::updateState(SState& state, const CBox& extent) {
    if (state.lastFrame == m_frame)
        return;

    static auto PFLUIDSPEED = CConfigValue<Config::FLOAT>("decoration:blur:fluid_jar:speed");
    static auto PFLUIDFILL  = CConfigValue<Config::FLOAT>("decoration:blur:fluid_jar:fill_amount");
    static auto PPRECISION  = CConfigValue<Config::FLOAT>("decoration:blur:fluid_jar:precision");

    const auto  precision      = std::clamp(*PPRECISION, MIN_PRECISION, MAX_PRECISION);
    const auto  simulationSize = fluidJarSimulationSize(extent.size(), precision);
    const auto  fillAmount     = std::clamp(*PFLUIDFILL, 0.F, 1.F);
    if (simulationSize.x <= 0 || simulationSize.y <= 0)
        return;

    const auto now     = Time::steadyNow();
    const auto elapsed = state.lastUpdate == Time::steady_tp{} ? FIXED_TIMESTEP : std::clamp(std::chrono::duration<float>(now - state.lastUpdate).count(), 0.F, 0.5F);
    const auto speed   = std::clamp(*PFLUIDSPEED, 0.F, 10.F);

    if (!state.particles[0] || state.fillAmount != fillAmount || state.precision != precision) {
        initializeState(state, simulationSize, fillAmount, precision);
        state.extent    = extent;
        state.hasExtent = true;
    } else if (!state.hasExtent) {
        state.extent    = extent;
        state.hasExtent = true;
    } else if (state.simulationSize != simulationSize || geometryChanged(state.extent, extent)) {
        const auto discontinuous = geometryDiscontinuous(state.extent, extent, elapsed);
        const auto transform     = fluidJarGeometryTransform(state.extent, extent, state.simulationSize, simulationSize, !discontinuous);
        state.wallVelocities     = discontinuous ? std::array<float, 3>{} : fluidJarWallVelocities(state.extent, extent, simulationSize, elapsed, speed);
        transformState(state, simulationSize, transform, state.wallVelocities);
        state.extent = extent;
    } else
        state.wallVelocities = {};

    if (speed > 0.F && state.particleCount > 0) {
        state.animationTime += std::min(elapsed, 0.05F) * speed;
        state.accumulator += std::min(elapsed, 0.05F) * speed;

        int substeps = 0;
        while (state.accumulator >= FIXED_TIMESTEP && substeps < MAX_SUBSTEPS) {
            drawParticleStep(state, SOLVER_TIMESTEP);
            drawGraphStep(state);
            drawTrackingStep(state);
            state.accumulator -= FIXED_TIMESTEP;
            ++state.simulationFrame;
            ++substeps;
        }

        if (substeps > 0)
            drawVisualStep(state, substeps);

        if (substeps == MAX_SUBSTEPS)
            state.accumulator = std::min(state.accumulator, sc<double>(FIXED_TIMESTEP));

        scheduleNextFrame(state);
    }

    state.lastUpdate = now;
    state.lastFrame  = m_frame;
}

void CFluidJarBlurProvider::initializeState(SState& state, const Vector2D& simulationSize, float fillAmount, float precision) {
    state.simulationSize      = simulationSize;
    state.gridSize            = fluidJarGridSize(simulationSize);
    state.particleTextureSize = {state.gridSize.x * 4.0, state.gridSize.y};
    state.graphTextureSize    = {state.gridSize.x * 8.0, state.gridSize.y};

    allocateBuffers(state.particles, state.particleTextureSize, "Fluid jar particles");
    allocateBuffers(state.graph, state.graphTextureSize, "Fluid jar graph", DRM_FORMAT_ABGR16161616);
    allocateBuffers(state.tracking, simulationSize, "Fluid jar tracking", DRM_FORMAT_ABGR16161616);
    allocateBuffers(state.visual, simulationSize, "Fluid jar visual");

    state.fillAmount       = fillAmount;
    state.precision        = precision;
    state.particleCount    = fluidJarInitialParticleCount(simulationSize, fillAmount);
    state.currentParticles = 0;
    state.currentGraph     = 0;
    state.currentTracking  = 0;
    state.currentVisual    = 0;
    state.simulationFrame  = 0;
    state.accumulator      = 0.0;
    state.animationTime    = 0.0;
    state.lastUpdate       = {};
    state.wallVelocities   = {};

    for (const auto& buffer : state.particles)
        drawInitialize(state, buffer);

    clearIntegerBuffers(state.graph);
    for (int i = 0; i < INITIAL_GRAPH_STEPS; ++i) {
        drawGraphStep(state);
        ++state.simulationFrame;
    }

    clearIntegerBuffers(state.tracking);
    clearBuffers(state.visual, {0.F, 0.F, 0.F, 0.F});
    if (state.particleCount > 0) {
        for (int i = 0; i < INITIAL_TRACK_STEPS; ++i) {
            drawTrackingStep(state);
            ++state.simulationFrame;
        }
        for (int i = 0; i < INITIAL_VISUAL_STEPS; ++i)
            drawVisualStep(state);
    }
}

void CFluidJarBlurProvider::transformState(SState& state, const Vector2D& simulationSize, const SFluidJarGeometryTransform& transform, const std::array<float, 3>& wallVelocities) {
    const auto oldSize          = state.simulationSize;
    const auto oldGridSize      = state.gridSize;
    const auto oldParticleCount = state.particleCount;
    const auto oldParticles     = state.particles[state.currentParticles];
    const auto oldTracking      = state.tracking[state.currentTracking];
    const auto oldVisual        = state.visual[state.currentVisual];
    const bool resized          = oldSize != simulationSize;

    state.simulationSize = simulationSize;
    state.particleCount  = fluidJarResizedParticleCount(oldParticleCount, simulationSize);

    const auto particleTarget = state.particles[1 - state.currentParticles];
    drawResample(state, oldParticles, particleTarget, oldGridSize, oldParticleCount, transform, wallVelocities);
    state.currentParticles = 1 - state.currentParticles;
    drawGraphStep(state);

    if (resized) {
        std::array<SP<CGLFramebuffer>, 2> tracking;
        std::array<SP<CGLFramebuffer>, 2> visual;
        allocateBuffers(tracking, simulationSize, "Fluid jar resized tracking", DRM_FORMAT_ABGR16161616);
        allocateBuffers(visual, simulationSize, "Fluid jar resized visual");
        clearIntegerBuffers(tracking);
        clearBuffers(visual, {0.F, 0.F, 0.F, 0.F});
        drawTrackingResample(oldTracking, tracking[0], oldSize, transform);
        drawHistoryResample(oldVisual, visual[0], oldSize, transform, {0.F, 0.F, 0.F, 0.F}, true);
        state.tracking        = std::move(tracking);
        state.visual          = std::move(visual);
        state.currentTracking = 0;
        state.currentVisual   = 0;
    } else {
        drawTrackingResample(oldTracking, state.tracking[1 - state.currentTracking], oldSize, transform);
        drawHistoryResample(oldVisual, state.visual[1 - state.currentVisual], oldSize, transform, {0.F, 0.F, 0.F, 0.F}, true);
        state.currentTracking = 1 - state.currentTracking;
        state.currentVisual   = 1 - state.currentVisual;
    }

    if (state.particleCount > 0) {
        drawTrackingStep(state);
        const bool velocityScaleChanged = std::abs(transform.velocityScale.x - 1.0) > 0.001 || std::abs(transform.velocityScale.y - 1.0) > 0.001;
        drawVisualStep(state, resized || velocityScaleChanged ? INITIAL_VISUAL_STEPS : 1);
    }
}

void CFluidJarBlurProvider::allocateBuffers(std::array<SP<CGLFramebuffer>, 2>& buffers, const Vector2D& size, const std::string& name, DRMFormat format) const {
    for (auto& buffer : buffers) {
        if (!buffer)
            buffer = dynamicPointerCast<CGLFramebuffer>(g_pHyprRenderer->createFB(name));
        buffer->alloc(sc<int>(size.x), sc<int>(size.y), format);
    }
}

void CFluidJarBlurProvider::clearIntegerBuffers(const std::array<SP<CGLFramebuffer>, 2>& buffers) const {
    constexpr std::array<GLuint, 4> CLEAR_VALUE = {};
    for (const auto& buffer : buffers) {
        buffer->bind();
        g_pHyprRenderer->disableScissor();
        g_pHyprRenderer->blend(false);
        glClearBufferuiv(GL_COLOR, 0, CLEAR_VALUE.data());
    }
}

void CFluidJarBlurProvider::clearBuffers(const std::array<SP<CGLFramebuffer>, 2>& buffers, const std::array<float, 4>& color) const {
    for (const auto& buffer : buffers) {
        buffer->bind();
        g_pHyprRenderer->setViewport(0, 0, sc<int>(buffer->m_size.x), sc<int>(buffer->m_size.y));
        g_pHyprRenderer->disableScissor();
        glClearColor(color[0], color[1], color[2], color[3]);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void CFluidJarBlurProvider::drawInitialize(const SState& state, SP<CGLFramebuffer> target) const {
    const auto shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_FLUIDJARINIT));
    preparePass(target, state.particleTextureSize, shader);
    shader->setUniformFloat2(SHADER_FLUIDJAR_RESOLUTION, state.simulationSize.x, state.simulationSize.y);
    shader->setUniformFloat2(SHADER_FLUIDJAR_GRID_SIZE, state.gridSize.x, state.gridSize.y);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_COUNT, state.particleCount);
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void CFluidJarBlurProvider::drawResample(const SState& state, SP<CGLFramebuffer> source, SP<CGLFramebuffer> target, const Vector2D& oldGridSize, int oldParticleCount,
                                         const SFluidJarGeometryTransform& transform, const std::array<float, 3>& wallVelocities) const {
    static auto PFLUIDMASS = CConfigValue<Config::FLOAT>("decoration:blur:fluid_jar:mass");

    bindNearestTexture(source, GL_TEXTURE0);
    const auto shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_FLUIDJARRESAMPLE));
    preparePass(target, state.particleTextureSize, shader);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_TEX, 0);
    shader->setUniformFloat2(SHADER_FLUIDJAR_RESOLUTION, state.simulationSize.x, state.simulationSize.y);
    shader->setUniformFloat2(SHADER_FLUIDJAR_GRID_SIZE, state.gridSize.x, state.gridSize.y);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_COUNT, state.particleCount);
    shader->setUniformFloat2(SHADER_FLUIDJAR_OLD_GRID_SIZE, oldGridSize.x, oldGridSize.y);
    shader->setUniformInt(SHADER_FLUIDJAR_OLD_PARTICLE_COUNT, oldParticleCount);
    shader->setUniformFloat4(SHADER_FLUIDJAR_TRANSFORM, transform.positionScale.x, transform.positionScale.y, transform.positionOffset.x, transform.positionOffset.y);
    shader->setUniformFloat2(SHADER_FLUIDJAR_VELOCITY_SCALE, transform.velocityScale.x, transform.velocityScale.y);
    shader->setUniformFloat3(SHADER_FLUIDJAR_WALL_VELOCITIES, wallVelocities[0], wallVelocities[1], wallVelocities[2]);
    shader->setUniformFloat(SHADER_FLUIDJAR_MASS, std::clamp(*PFLUIDMASS, 0.1F, 10.F));
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void CFluidJarBlurProvider::drawHistoryResample(SP<CGLFramebuffer> source, SP<CGLFramebuffer> target, const Vector2D& oldSize, const SFluidJarGeometryTransform& transform,
                                                const std::array<float, 4>& fallback, bool linear) const {
    glActiveTexture(GL_TEXTURE0);
    const auto texture = source->getTexture();
    texture->bind();
    texture->setTexParameter(GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    texture->setTexParameter(GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);

    const Vector2D inverseScale  = {1.0 / transform.positionScale.x, 1.0 / transform.positionScale.y};
    const Vector2D inverseOffset = {-transform.positionOffset.x * inverseScale.x, -transform.positionOffset.y * inverseScale.y};
    const auto     shader        = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_FLUIDJARHISTORYRESAMPLE));
    preparePass(target, target->m_size, shader);
    shader->setUniformInt(SHADER_FLUIDJAR_HISTORY_TEX, 0);
    shader->setUniformFloat2(SHADER_FLUIDJAR_OLD_RESOLUTION, oldSize.x, oldSize.y);
    shader->setUniformFloat4(SHADER_FLUIDJAR_HISTORY_TRANSFORM, inverseScale.x, inverseScale.y, inverseOffset.x, inverseOffset.y);
    shader->setUniformFloat4(SHADER_FLUIDJAR_HISTORY_FALLBACK, fallback[0], fallback[1], fallback[2], fallback[3]);
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void CFluidJarBlurProvider::drawParticleStep(SState& state, float dt) const {
    static auto PFLUIDMASS = CConfigValue<Config::FLOAT>("decoration:blur:fluid_jar:mass");

    const auto  source = state.particles[state.currentParticles];
    const auto  target = state.particles[1 - state.currentParticles];
    bindNearestTexture(source, GL_TEXTURE0);
    bindNearestTexture(state.graph[state.currentGraph], GL_TEXTURE1);
    const auto shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_FLUIDJARSTEP));
    preparePass(target, state.particleTextureSize, shader);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_TEX, 0);
    shader->setUniformInt(SHADER_FLUIDJAR_GRAPH_TEX, 1);
    shader->setUniformFloat2(SHADER_FLUIDJAR_RESOLUTION, state.simulationSize.x, state.simulationSize.y);
    shader->setUniformFloat2(SHADER_FLUIDJAR_GRID_SIZE, state.gridSize.x, state.gridSize.y);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_COUNT, state.particleCount);
    shader->setUniformFloat(SHADER_FLUIDJAR_DT, dt);
    shader->setUniformFloat(SHADER_FLUIDJAR_MASS, std::clamp(*PFLUIDMASS, 0.1F, 10.F));
    shader->setUniformFloat3(SHADER_FLUIDJAR_WALL_VELOCITIES, state.wallVelocities[0], state.wallVelocities[1], state.wallVelocities[2]);
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    state.currentParticles = 1 - state.currentParticles;
}

void CFluidJarBlurProvider::drawGraphStep(SState& state) const {
    const auto target = state.graph[1 - state.currentGraph];
    bindNearestTexture(state.particles[state.currentParticles], GL_TEXTURE0);
    bindNearestTexture(state.graph[state.currentGraph], GL_TEXTURE1);
    const auto shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_FLUIDJARGRAPH));
    preparePass(target, state.graphTextureSize, shader);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_TEX, 0);
    shader->setUniformInt(SHADER_FLUIDJAR_GRAPH_TEX, 1);
    shader->setUniformFloat2(SHADER_FLUIDJAR_RESOLUTION, state.simulationSize.x, state.simulationSize.y);
    shader->setUniformFloat2(SHADER_FLUIDJAR_GRID_SIZE, state.gridSize.x, state.gridSize.y);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_COUNT, state.particleCount);
    shader->setUniformInt(SHADER_FLUIDJAR_FRAME, sc<int>(state.simulationFrame));
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    state.currentGraph = 1 - state.currentGraph;
}

void CFluidJarBlurProvider::drawTrackingStep(SState& state) const {
    const auto target = state.tracking[1 - state.currentTracking];
    bindNearestTexture(state.particles[state.currentParticles], GL_TEXTURE0);
    bindNearestTexture(state.graph[state.currentGraph], GL_TEXTURE1);
    bindNearestTexture(state.tracking[state.currentTracking], GL_TEXTURE2);
    const auto shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_FLUIDJARTRACK));
    preparePass(target, state.simulationSize, shader);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_TEX, 0);
    shader->setUniformInt(SHADER_FLUIDJAR_GRAPH_TEX, 1);
    shader->setUniformInt(SHADER_FLUIDJAR_TRACKING_TEX, 2);
    shader->setUniformFloat2(SHADER_FLUIDJAR_RESOLUTION, state.simulationSize.x, state.simulationSize.y);
    shader->setUniformFloat2(SHADER_FLUIDJAR_GRID_SIZE, state.gridSize.x, state.gridSize.y);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_COUNT, state.particleCount);
    shader->setUniformInt(SHADER_FLUIDJAR_FRAME, sc<int>(state.simulationFrame));
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    state.currentTracking = 1 - state.currentTracking;
}

void CFluidJarBlurProvider::drawTrackingResample(SP<CGLFramebuffer> source, SP<CGLFramebuffer> target, const Vector2D& oldSize, const SFluidJarGeometryTransform& transform) const {
    bindNearestTexture(source, GL_TEXTURE0);

    const Vector2D inverseScale  = {1.0 / transform.positionScale.x, 1.0 / transform.positionScale.y};
    const Vector2D inverseOffset = {-transform.positionOffset.x * inverseScale.x, -transform.positionOffset.y * inverseScale.y};
    const auto     shader        = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_FLUIDJARTRACKINGRESAMPLE));
    preparePass(target, target->m_size, shader);
    shader->setUniformInt(SHADER_FLUIDJAR_HISTORY_TEX, 0);
    shader->setUniformFloat2(SHADER_FLUIDJAR_OLD_RESOLUTION, oldSize.x, oldSize.y);
    shader->setUniformFloat4(SHADER_FLUIDJAR_HISTORY_TRANSFORM, inverseScale.x, inverseScale.y, inverseOffset.x, inverseOffset.y);
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void CFluidJarBlurProvider::drawVisualStep(SState& state, int steps) const {
    const auto target = state.visual[1 - state.currentVisual];
    bindNearestTexture(state.particles[state.currentParticles], GL_TEXTURE0);
    bindNearestTexture(state.graph[state.currentGraph], GL_TEXTURE1);
    bindNearestTexture(state.tracking[state.currentTracking], GL_TEXTURE2);
    bindNearestTexture(state.visual[state.currentVisual], GL_TEXTURE3);
    const auto shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_FLUIDJARVISUAL));
    preparePass(target, state.simulationSize, shader);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_TEX, 0);
    shader->setUniformInt(SHADER_FLUIDJAR_GRAPH_TEX, 1);
    shader->setUniformInt(SHADER_FLUIDJAR_TRACKING_TEX, 2);
    shader->setUniformInt(SHADER_FLUIDJAR_VISUAL_TEX, 3);
    shader->setUniformFloat2(SHADER_FLUIDJAR_RESOLUTION, state.simulationSize.x, state.simulationSize.y);
    shader->setUniformFloat2(SHADER_FLUIDJAR_GRID_SIZE, state.gridSize.x, state.gridSize.y);
    shader->setUniformInt(SHADER_FLUIDJAR_PARTICLE_COUNT, state.particleCount);
    shader->setUniformFloat(SHADER_FLUIDJAR_VISUAL_RESPONSE, sc<float>(std::max(steps, 1)));
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    state.currentVisual = 1 - state.currentVisual;
    glActiveTexture(GL_TEXTURE0);
}

void CFluidJarBlurProvider::preparePass(SP<CGLFramebuffer> target, const Vector2D& size, WP<CShader> shader) const {
    target->bind();
    g_pHyprRenderer->setViewport(0, 0, sc<int>(size.x), sc<int>(size.y));
    g_pHyprRenderer->disableScissor();
    g_pHyprRenderer->blend(false);
    const auto monitor   = g_pHyprRenderer->m_renderData.pMonitor;
    const auto transform = Math::wlTransformToHyprutils(Math::invertTransform(monitor->m_transform));
    const auto matrix    = g_pHyprRenderer->projectBoxToTarget({0, 0, monitor->m_transformedSize.x, monitor->m_transformedSize.y}, transform);
    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, matrix.getMatrix());
}

CBox CFluidJarBlurProvider::transformedPatternBox(const SBlurContext& context) const {
    const auto monitor = g_pHyprRenderer->m_renderData.pMonitor;
    if (!monitor)
        return {};

    CBox box = context.patternBox.value_or(CBox{0, 0, monitor->m_transformedSize.x, monitor->m_transformedSize.y});
    if (context.patternBox)
        box.transform(Math::wlTransformToHyprutils(Math::invertTransform(monitor->m_transform)), monitor->m_transformedSize.x, monitor->m_transformedSize.y);
    return box;
}

void CFluidJarBlurProvider::scheduleNextFrame(const SState& state) const {
    const auto window = state.window.lock();
    if (!window)
        return;
    g_pHyprRenderer->damageWindow(window);
}

void CFluidJarBlurProvider::pruneStates() {
    std::erase_if(m_states, [](const auto& state) { return state.window.expired(); });
}

Vector2D Render::GL::fluidJarSimulationSize(const Vector2D& extent, float precision) {
    if (extent.x <= 0 || extent.y <= 0)
        return {};

    const auto clampedPrecision = std::clamp(precision, MIN_PRECISION, MAX_PRECISION);
    const auto simulationScale  = BASE_SIMULATION_SCALE * clampedPrecision;
    const auto maxSide          = BASE_SIMULATION_SIDE * clampedPrecision;
    const auto scale            = std::min({sc<double>(simulationScale), sc<double>(maxSide) / extent.x, sc<double>(maxSide) / extent.y});
    Vector2D   size             = {std::max(8.0, std::floor(extent.x * scale)), std::max(4.0, std::floor(extent.y * scale))};

    const auto capacity = fluidJarParticleCapacity(size);
    if (capacity > MAX_PARTICLES) {
        const auto particleScale = std::sqrt(sc<double>(MAX_PARTICLES) / capacity);
        size.x                   = std::max(8.0, std::floor(size.x * particleScale));
        size.y                   = std::max(4.0, std::floor(size.y * particleScale));
    }

    while (fluidJarParticleCapacity(size) > MAX_PARTICLES) {
        if (size.x >= size.y)
            size.x -= 1.0;
        else
            size.y -= 1.0;
    }

    return size;
}

Vector2D Render::GL::fluidJarGridSize(const Vector2D& simulationSize) {
    return {std::max(1.0, std::floor(simulationSize.x * 0.5 / 4.0)), std::max(1.0, std::floor(simulationSize.y * 0.5 / 2.0))};
}

int Render::GL::fluidJarParticleCapacity(const Vector2D& simulationSize) {
    const auto grid = fluidJarGridSize(simulationSize);
    return sc<int>(grid.x * grid.y);
}

int Render::GL::fluidJarInitialParticleCount(const Vector2D& simulationSize, float fillAmount) {
    return sc<int>(std::floor(fluidJarParticleCapacity(simulationSize) * std::clamp(fillAmount, 0.F, 1.F)));
}

int Render::GL::fluidJarResizedParticleCount(int oldParticleCount, const Vector2D&) {
    return std::clamp(oldParticleCount, 0, MAX_PARTICLES);
}

float Render::GL::fluidJarDamageRadius(int64_t size, int64_t passes, float distortion) {
    return dualKawaseDamageRadius(size, passes) + std::ceil(MAX_REFRACTION * std::clamp(distortion, 0.F, MAX_DISTORTION));
}

SFluidJarGeometryTransform Render::GL::fluidJarGeometryTransform(const CBox& oldExtent, const CBox& newExtent, const Vector2D& oldSimulationSize, const Vector2D& newSimulationSize,
                                                                 bool preserveWorldPosition) {
    if (oldExtent.width <= 0 || oldExtent.height <= 0 || newExtent.width <= 0 || newExtent.height <= 0 || oldSimulationSize.x <= 0 || oldSimulationSize.y <= 0 ||
        newSimulationSize.x <= 0 || newSimulationSize.y <= 0)
        return {};

    const Vector2D oldScale = {oldSimulationSize.x / oldExtent.width, oldSimulationSize.y / oldExtent.height};
    const Vector2D newScale = {newSimulationSize.x / newExtent.width, newSimulationSize.y / newExtent.height};
    const Vector2D scale    = {newScale.x / oldScale.x, newScale.y / oldScale.y};
    if (!preserveWorldPosition)
        return {.positionScale = {newSimulationSize.x / oldSimulationSize.x, newSimulationSize.y / oldSimulationSize.y}, .velocityScale = {0, 0}};

    const auto oldBottom = oldExtent.y + oldExtent.height;
    const auto newBottom = newExtent.y + newExtent.height;
    return {
        .positionScale  = scale,
        .positionOffset = {(oldExtent.x - newExtent.x) * newScale.x, (newBottom - oldBottom) * newScale.y},
        .velocityScale  = scale,
    };
}

std::array<float, 3> Render::GL::fluidJarWallVelocities(const CBox& oldExtent, const CBox& newExtent, const Vector2D& simulationSize, float elapsed, float speed) {
    if (elapsed <= 0.F || oldExtent.width <= 0 || oldExtent.height <= 0 || newExtent.width <= 0 || newExtent.height <= 0 || simulationSize.x <= 0 || simulationSize.y <= 0)
        return {};

    if (speed <= 0.F)
        return {};

    const auto scaleX      = simulationSize.x / newExtent.width;
    const auto scaleY      = simulationSize.y / newExtent.height;
    const auto oldRight    = oldExtent.x + oldExtent.width;
    const auto newRight    = newExtent.x + newExtent.width;
    const auto oldBottom   = oldExtent.y + oldExtent.height;
    const auto newBottom   = newExtent.y + newExtent.height;
    const auto solverScale = FIXED_TIMESTEP / (SOLVER_TIMESTEP * speed);
    const auto velocity = [&](double displacement, double scale) { return std::clamp(sc<float>(displacement / elapsed * scale * solverScale), -MAX_WALL_SPEED, MAX_WALL_SPEED); };

    return {
        velocity(newExtent.x - oldExtent.x, scaleX),
        velocity(newRight - oldRight, scaleX),
        velocity(oldBottom - newBottom, scaleY),
    };
}
