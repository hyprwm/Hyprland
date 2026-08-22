#include "Acrylic.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../helpers/Color.hpp"
#include "../../../helpers/cm/ColorManagement.hpp"

#include <algorithm>
#include <cmath>

using namespace Render;
using namespace Render::GL;
using namespace NColorManagement;

static constexpr float MAX_ACRYLIC_REFRACTION = 48.F;
static constexpr float MIN_ACRYLIC_BULB       = 4.F;
static constexpr float MAX_ACRYLIC_BULB       = 256.F;

static float           acrylicSampleRadius(float refraction) {
    const auto CLAMPED = std::clamp(refraction, 0.F, MAX_ACRYLIC_REFRACTION);
    return CLAMPED > 0.F ? std::ceil(CLAMPED + 1.F) : 0.F;
}

static float srgbToLinear(float value) {
    return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

static float acrylicLuminanceScale() {
    const auto INTERMEDIATE = getDefaultImageDescription();
    const auto WORKBUFFER   = g_pHyprRenderer->workBufferImageDescription();
    if (!WORKBUFFER)
        return 1.F;

    const auto MINIMUM = INTERMEDIATE->value().getTFMinLuminance();
    const auto MAXIMUM = INTERMEDIATE->value().getTFMaxLuminance();
    const auto RANGE   = std::max(MAXIMUM, sc<float>(WORKBUFFER->value().luminances.max)) - MINIMUM;
    return (MAXIMUM - MINIMUM) / std::max(RANGE, 0.001F);
}

CAcrylicBlurProvider::CAcrylicBlurProvider(CHyprOpenGLImpl& impl) : CDualKawaseBlurProvider(impl, makeUnique<CAcrylicBlurMaterial>()) {
    ;
}

eBlurType CAcrylicBlurMaterial::type() const noexcept {
    return eBlurType::BLUR_ACRYLIC;
}

SBlurMaterialRequirements CAcrylicBlurMaterial::requirements() const noexcept {
    return {
        .finishFragment = SH_FRAG_ACRYLICFINISH,
        .preparedInput  = true,
        .liveBlur       = true,
    };
}

float CAcrylicBlurMaterial::sampleRadius() const {
    static auto PACRYLICREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:acrylic:refraction");
    return acrylicSampleRadius(*PACRYLICREFRACTION);
}

void CAcrylicBlurMaterial::bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const {
    shader->setUniformInt(SHADER_ACRYLIC_ENABLED, 0);

    if (!context.blurContext.shape)
        return;

    const auto extent = context.blurContext.shape->box;
    if (extent.width <= 0 || extent.height <= 0)
        return;

    static auto PACRYLICREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:acrylic:refraction");
    static auto PACRYLICBULB       = CConfigValue<Config::FLOAT>("decoration:blur:acrylic:bulb");
    static auto PACRYLICCLARITY    = CConfigValue<Config::FLOAT>("decoration:blur:acrylic:clarity");
    static auto PACRYLICABERRATION = CConfigValue<Config::FLOAT>("decoration:blur:acrylic:aberration");
    static auto PACRYLICTINT       = CConfigValue<Config::INTEGER>("decoration:blur:acrylic:tint");

    const auto  TINT            = CHyprColor(*PACRYLICTINT);
    const auto  TINT_ALPHA      = std::clamp(sc<float>(TINT.a), 0.F, 1.F);
    const auto  TINT_DEPTH      = -std::log(std::max(1.F - TINT_ALPHA, 0.0001F));
    const auto  LUMINANCE_SCALE = acrylicLuminanceScale();

    shader->setUniformInt(SHADER_ACRYLIC_ENABLED, 1);
    shader->setUniformFloat4(SHADER_ACRYLIC_EXTENT, sc<float>(extent.x), sc<float>(extent.y), sc<float>(extent.width), sc<float>(extent.height));
    shader->setUniformFloat(SHADER_ACRYLIC_RADIUS, std::max(context.blurContext.shape->radius, 0.F));
    shader->setUniformFloat(SHADER_ACRYLIC_ROUNDING_POWER, std::max(context.blurContext.shape->roundingPower, 1.F));
    shader->setUniformFloat(SHADER_ACRYLIC_REFRACTION, std::clamp(*PACRYLICREFRACTION, 0.F, MAX_ACRYLIC_REFRACTION));
    shader->setUniformFloat(SHADER_ACRYLIC_BULB, std::clamp(*PACRYLICBULB, MIN_ACRYLIC_BULB, MAX_ACRYLIC_BULB));
    shader->setUniformFloat(SHADER_ACRYLIC_CLARITY, std::clamp(*PACRYLICCLARITY, 0.F, 1.F));
    shader->setUniformFloat(SHADER_ACRYLIC_ABERRATION, std::clamp(*PACRYLICABERRATION, 0.F, 0.25F));
    shader->setUniformFloat4(SHADER_ACRYLIC_TINT, srgbToLinear(sc<float>(TINT.r)) * LUMINANCE_SCALE, srgbToLinear(sc<float>(TINT.g)) * LUMINANCE_SCALE,
                             srgbToLinear(sc<float>(TINT.b)) * LUMINANCE_SCALE, TINT_DEPTH);
    shader->setUniformFloat(SHADER_ACRYLIC_STRENGTH, std::clamp(context.strength, 0.F, 1.F));
    shader->setUniformInt(SHADER_ACRYLIC_TRANSFER_FUNCTION, sc<int>(getDefaultImageDescription()->value().transferFunction));
    shader->setUniformFloat(SHADER_ACRYLIC_LUMINANCE_SCALE, LUMINANCE_SCALE);
}

float Render::GL::acrylicDamageRadius(int64_t size, int64_t passes, float refraction) {
    return dualKawaseDamageRadius(size, passes) + acrylicSampleRadius(refraction);
}
