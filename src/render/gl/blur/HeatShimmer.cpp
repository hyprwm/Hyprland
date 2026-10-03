#include "HeatShimmer.hpp"

#include "../../Context.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../event/EventBus.hpp"

#include <algorithm>
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

CHeatShimmerBlurMaterial::CHeatShimmerBlurMaterial() : CGlassBlurMaterial(eBlurType::BLUR_HEAT_SHIMMER, SH_FRAG_HEATSHIMMERFINISH) {
    m_configListener = Event::bus()->m_events.config.props_refreshed.listen([this](const bool) {
        const auto SPEED = heatShimmerSpeed();
        m_animationClock.update(Time::steadyNow(), SPEED);
    });
}

CHeatShimmerBlurProvider::CHeatShimmerBlurProvider(CHyprOpenGLImpl& impl) : CGlassBlurProvider(impl, makeUnique<CHeatShimmerBlurMaterial>()) {
    ;
}

bool CHeatShimmerBlurMaterial::isAnimated(CRenderContext& ctx) const noexcept {
    static auto PBLURENABLED     = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");
    static auto PGLASSROUGHNESS  = CConfigValue<Config::FLOAT>("decoration:blur:glass:roughness");

    const auto  SPEED = heatShimmerSpeed();
    if (!ctx.readOnlyEffects())
        m_animationClock.update(ctx.effectTime(), SPEED);
    return *PBLURENABLED && SPEED > 0.F && (*PGLASSREFRACTION > 0.F || *PGLASSROUGHNESS > 0.F);
}

void CHeatShimmerBlurMaterial::bindFinish(CRenderContext& ctx, WP<CShader> shader, const SBlurMaterialContext& context) const {
    CGlassBlurMaterial::bindFinish(ctx, shader, context);
    shader->setUniformFloat(SHADER_TIME, animationPhase(ctx));
}

float CHeatShimmerBlurMaterial::animationPhase(CRenderContext& ctx) const {
    const auto SPEED = heatShimmerSpeed();
    const auto NOW   = ctx.effectTime();
    const auto TIME  = ctx.readOnlyEffects() ? m_animationClock.sample(NOW) : m_animationClock.update(NOW, SPEED);
    if (SPEED <= 0.F)
        return 0.F;

    return sc<float>(std::fmod(TIME * HEAT_SHIMMER_BASE_SPEED, HEAT_SHIMMER_PERIOD));
}
