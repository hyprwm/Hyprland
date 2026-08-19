#include "HeatShimmer.hpp"

#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../event/EventBus.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace Render;
using namespace Render::GL;

static constexpr float  MAX_HEAT_SHIMMER_SPEED  = 10.F;
static constexpr double HEAT_SHIMMER_BASE_SPEED = 0.8;
static constexpr double HEAT_SHIMMER_PERIOD     = 6.283185307179586;

static float            heatShimmerSpeed() {
    static auto PHEATSHIMMERSPEED = CConfigValue<Config::FLOAT>("decoration:blur:heat_shimmer:speed");
    return std::clamp(*PHEATSHIMMERSPEED, 0.F, MAX_HEAT_SHIMMER_SPEED);
}

CHeatShimmerBlurMaterial::CHeatShimmerBlurMaterial() : CGlassBlurMaterial(eBlurType::BLUR_HEAT_SHIMMER, SH_FRAG_HEATSHIMMERFINISH), m_lastAnimationUpdate(Time::steadyNow()) {
    m_configListener = Event::bus()->m_events.config.props_refreshed.listen([this](const bool) { updateAnimation(heatShimmerSpeed()); });
}

CHeatShimmerBlurProvider::CHeatShimmerBlurProvider(CHyprOpenGLImpl& impl) : CGlassBlurProvider(impl, makeUnique<CHeatShimmerBlurMaterial>()) {
    ;
}

bool CHeatShimmerBlurMaterial::isAnimated(const CRenderingContext&) const noexcept {
    static auto PBLURENABLED     = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");
    static auto PGLASSROUGHNESS  = CConfigValue<Config::FLOAT>("decoration:blur:glass:roughness");

    const auto  SPEED = heatShimmerSpeed();
    updateAnimation(SPEED);
    return *PBLURENABLED && SPEED > 0.F && (*PGLASSREFRACTION > 0.F || *PGLASSROUGHNESS > 0.F);
}

void CHeatShimmerBlurMaterial::bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const {
    CGlassBlurMaterial::bindFinish(shader, context);
    updateAnimation(heatShimmerSpeed());
    shader->setUniformFloat(SHADER_TIME, animationPhase());
}

void CHeatShimmerBlurMaterial::updateAnimation(float speed) const {
    const auto NOW = Time::steadyNow();
    m_animationTime += std::chrono::duration<double>(NOW - m_lastAnimationUpdate).count() * m_previousSpeed;
    m_lastAnimationUpdate = NOW;
    m_previousSpeed       = speed;
}

float CHeatShimmerBlurMaterial::animationPhase() const {
    if (m_previousSpeed <= 0.F)
        return 0.F;

    return sc<float>(std::fmod(m_animationTime * HEAT_SHIMMER_BASE_SPEED, HEAT_SHIMMER_PERIOD));
}
