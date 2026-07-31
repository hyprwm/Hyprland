#include "Aurora.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../helpers/Color.hpp"
#include "../../../helpers/cm/ColorManagement.hpp"

#include <algorithm>
#include <chrono>
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

static float auroraLuminanceScale() {
    const auto INTERMEDIATE = getDefaultImageDescription();
    const auto WORKBUFFER   = g_pHyprRenderer->workBufferImageDescription();
    if (!WORKBUFFER)
        return 1.F;

    const auto MINIMUM = INTERMEDIATE->value().getTFMinLuminance();
    const auto MAXIMUM = INTERMEDIATE->value().getTFMaxLuminance();
    const auto RANGE   = std::max(MAXIMUM, sc<float>(WORKBUFFER->value().luminances.max)) - MINIMUM;
    return (MAXIMUM - MINIMUM) / std::max(RANGE, 0.001F);
}

CAuroraBlurMaterial::CAuroraBlurMaterial() : CGlassBlurMaterial(eBlurType::BLUR_AURORA, SH_FRAG_AURORAFINISH), m_lastAnimationUpdate(Time::steadyNow()) {
    m_configListener = Event::bus()->m_events.config.props_refreshed.listen([this](const bool) { updateAnimation(auroraSpeed()); });
}

CAuroraBlurProvider::CAuroraBlurProvider(CHyprOpenGLImpl& impl) : CGlassBlurProvider(impl, makeUnique<CAuroraBlurMaterial>()) {
    ;
}

bool CAuroraBlurMaterial::isAnimated() const noexcept {
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
    updateAnimation(SPEED);
    return *PBLURENABLED && SPEED > 0.F && (HAS_COLOR || *PGLASSREFRACTION > 0.F || *PGLASSROUGHNESS > 0.F);
}

void CAuroraBlurMaterial::bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const {
    static auto PAURORAINTENSITY = CConfigValue<Config::FLOAT>("decoration:blur:aurora:intensity");
    static auto PAURORACOLOR1    = CConfigValue<Config::INTEGER>("decoration:blur:aurora:color1");
    static auto PAURORACOLOR2    = CConfigValue<Config::INTEGER>("decoration:blur:aurora:color2");

    CGlassBlurMaterial::bindFinish(shader, context);
    updateAnimation(auroraSpeed());

    const auto COLOR1 = CHyprColor(*PAURORACOLOR1);
    const auto COLOR2 = CHyprColor(*PAURORACOLOR2);
    const auto SCALE  = auroraLuminanceScale();

    const auto bindColor = [&](eShaderUniform uniform, const CHyprColor& color) {
        const auto ALPHA = sc<float>(color.a);
        shader->setUniformFloat4(uniform, srgbToLinear(sc<float>(color.r)) * SCALE * ALPHA, srgbToLinear(sc<float>(color.g)) * SCALE * ALPHA,
                                 srgbToLinear(sc<float>(color.b)) * SCALE * ALPHA, ALPHA);
    };

    shader->setUniformFloat(SHADER_TIME, animationPhase());
    shader->setUniformFloat(SHADER_AURORA_INTENSITY, std::clamp(*PAURORAINTENSITY, 0.F, 1.F) * std::clamp(context.strength, 0.F, 1.F));
    bindColor(SHADER_AURORA_COLOR1, COLOR1);
    bindColor(SHADER_AURORA_COLOR2, COLOR2);
    shader->setUniformInt(SHADER_AURORA_TRANSFER_FUNCTION, sc<int>(getDefaultImageDescription()->value().transferFunction));
}

void CAuroraBlurMaterial::updateAnimation(float speed) const {
    const auto NOW = Time::steadyNow();
    m_animationTime += std::chrono::duration<double>(NOW - m_lastAnimationUpdate).count() * m_previousSpeed;
    m_lastAnimationUpdate = NOW;
    m_previousSpeed       = speed;
}

float CAuroraBlurMaterial::animationPhase() const {
    return sc<float>(std::fmod(m_animationTime * AURORA_BASE_SPEED, AURORA_PERIOD));
}
