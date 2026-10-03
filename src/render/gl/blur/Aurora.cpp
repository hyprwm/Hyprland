#include "Aurora.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../helpers/Color.hpp"
#include "../../../helpers/cm/ColorManagement.hpp"

#include <algorithm>
#include <cmath>

using namespace Render;
using namespace Render::GL;
using namespace NColorManagement;

static constexpr float  MAX_AURORA_SPEED  = 10.F;
static constexpr double AURORA_BASE_SPEED = 0.22;
static constexpr double AURORA_PERIOD     = 6.283185307179586;

static float            auroraSpeed() {
    static auto PAURORASPEED = CConfigValue<Config::FLOAT>("decoration:blur:aurora:speed");
    return std::clamp(*PAURORASPEED, 0.F, MAX_AURORA_SPEED);
}

static float srgbToLinear(float value) {
    return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

static float auroraLuminanceScale(CRenderContext& ctx) {
    const auto INTERMEDIATE = getDefaultImageDescription();
    const auto WORKBUFFER   = g_pHyprRenderer->workBufferImageDescription(ctx);
    if (!WORKBUFFER)
        return 1.F;

    const auto MINIMUM = INTERMEDIATE->value().getTFMinLuminance();
    const auto MAXIMUM = INTERMEDIATE->value().getTFMaxLuminance();
    const auto RANGE   = std::max(MAXIMUM, sc<float>(WORKBUFFER->value().luminances.max)) - MINIMUM;
    return (MAXIMUM - MINIMUM) / std::max(RANGE, 0.001F);
}

CAuroraBlurMaterial::CAuroraBlurMaterial() : CGlassBlurMaterial(eBlurType::BLUR_AURORA, SH_FRAG_AURORAFINISH) {
    m_configListener = Event::bus()->m_events.config.props_refreshed.listen([this](const bool) {
        const auto SPEED = auroraSpeed();
        m_animationClock.update(Time::steadyNow(), SPEED);
    });
}

CAuroraBlurProvider::CAuroraBlurProvider(CHyprOpenGLImpl& impl) : CGlassBlurProvider(impl, makeUnique<CAuroraBlurMaterial>()) {
    ;
}

bool CAuroraBlurMaterial::isAnimated(CRenderContext& ctx) const noexcept {
    static auto PBLURENABLED     = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");
    static auto PGLASSROUGHNESS  = CConfigValue<Config::FLOAT>("decoration:blur:glass:roughness");
    static auto PAURORAINTENSITY = CConfigValue<Config::FLOAT>("decoration:blur:aurora:intensity");
    static auto PAURORACOLOR1    = CConfigValue<Config::INTEGER>("decoration:blur:aurora:color1");
    static auto PAURORACOLOR2    = CConfigValue<Config::INTEGER>("decoration:blur:aurora:color2");

    const auto  SPEED     = auroraSpeed();
    const auto  COLOR1    = CHyprColor(*PAURORACOLOR1);
    const auto  COLOR2    = CHyprColor(*PAURORACOLOR2);
    const bool  HAS_COLOR = *PAURORAINTENSITY > 0.F && (COLOR1.a > 0.F || COLOR2.a > 0.F);
    if (!ctx.readOnlyEffects())
        m_animationClock.update(ctx.effectTime(), SPEED);
    return *PBLURENABLED && SPEED > 0.F && (HAS_COLOR || *PGLASSREFRACTION > 0.F || *PGLASSROUGHNESS > 0.F);
}

void CAuroraBlurMaterial::bindFinish(CRenderContext& ctx, WP<CShader> shader, const SBlurMaterialContext& context) const {
    static auto PAURORAINTENSITY = CConfigValue<Config::FLOAT>("decoration:blur:aurora:intensity");
    static auto PAURORACOLOR1    = CConfigValue<Config::INTEGER>("decoration:blur:aurora:color1");
    static auto PAURORACOLOR2    = CConfigValue<Config::INTEGER>("decoration:blur:aurora:color2");

    CGlassBlurMaterial::bindFinish(ctx, shader, context);
    const auto PHASE = animationPhase(ctx);

    const auto COLOR1 = CHyprColor(*PAURORACOLOR1);
    const auto COLOR2 = CHyprColor(*PAURORACOLOR2);
    const auto SCALE  = auroraLuminanceScale(ctx);

    const auto bindColor = [&](eShaderUniform uniform, const CHyprColor& color) {
        const auto ALPHA = sc<float>(color.a);
        shader->setUniformFloat4(uniform, srgbToLinear(sc<float>(color.r)) * SCALE * ALPHA, srgbToLinear(sc<float>(color.g)) * SCALE * ALPHA,
                                 srgbToLinear(sc<float>(color.b)) * SCALE * ALPHA, ALPHA);
    };

    shader->setUniformFloat(SHADER_TIME, PHASE);
    shader->setUniformFloat(SHADER_AURORA_INTENSITY, std::clamp(*PAURORAINTENSITY, 0.F, 1.F) * std::clamp(context.strength, 0.F, 1.F));
    bindColor(SHADER_AURORA_COLOR1, COLOR1);
    bindColor(SHADER_AURORA_COLOR2, COLOR2);
    shader->setUniformInt(SHADER_AURORA_TRANSFER_FUNCTION, sc<int>(getDefaultImageDescription()->value().transferFunction));
}

float CAuroraBlurMaterial::animationPhase(CRenderContext& ctx) const {
    const auto SPEED = auroraSpeed();
    const auto NOW   = ctx.effectTime();
    const auto TIME  = ctx.readOnlyEffects() ? m_animationClock.sample(NOW) : m_animationClock.update(NOW, SPEED);
    return sc<float>(std::fmod(TIME * AURORA_BASE_SPEED, AURORA_PERIOD));
}
