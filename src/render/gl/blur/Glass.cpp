#include "Glass.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"

#include <algorithm>
#include <cmath>

using namespace Render;
using namespace Render::GL;

static constexpr float MAX_GLASS_REFRACTION = 20.F;

CGlassBlurProvider::CGlassBlurProvider(CHyprOpenGLImpl& impl, eBlurType type, ePreparedFragmentShader finishFragment) :
    CDualKawaseBlurProvider(impl), m_type(type), m_finishFragment(finishFragment) {
    ;
}

eBlurType CGlassBlurProvider::type() const noexcept {
    return m_type;
}

ePreparedFragmentShader CGlassBlurProvider::finishFragment() const noexcept {
    return m_finishFragment;
}

void CGlassBlurProvider::setFinishUniforms(WP<CShader> shader, float strength, const SBlurContext& context) const {
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");
    static auto PGLASSSIZE       = CConfigValue<Config::FLOAT>("decoration:blur:glass:size");
    static auto PGLASSROUGHNESS  = CConfigValue<Config::FLOAT>("decoration:blur:glass:roughness");

    const auto  clampedStrength = std::clamp(strength, 0.F, 1.F);

    shader->setUniformFloat(SHADER_GLASS_REFRACTION, std::clamp(*PGLASSREFRACTION, 0.F, MAX_GLASS_REFRACTION) * clampedStrength);
    shader->setUniformFloat(SHADER_GLASS_SIZE, std::clamp(*PGLASSSIZE, 4.F, 512.F));
    shader->setUniformFloat(SHADER_GLASS_ROUGHNESS, std::clamp(*PGLASSROUGHNESS, 0.F, 1.F) * clampedStrength);

    CBox transformedPatternBox{};
    if (context.patternBox && g_pHyprRenderer->m_renderData.pMonitor) {
        const auto MONITOR    = g_pHyprRenderer->m_renderData.pMonitor;
        transformedPatternBox = *context.patternBox;
        transformedPatternBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(MONITOR->m_transform)), MONITOR->m_transformedSize.x, MONITOR->m_transformedSize.y);
    }

    shader->setUniformFloat2(SHADER_GLASS_POSITION, sc<float>(transformedPatternBox.x), sc<float>(transformedPatternBox.y));
}

float CGlassBlurProvider::damageRadius() const {
    static auto PBLURSIZE        = CConfigValue<Config::INTEGER>("decoration:blur:size");
    static auto PBLURPASSES      = CConfigValue<Config::INTEGER>("decoration:blur:passes");
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");

    return glassDamageRadius(*PBLURSIZE, *PBLURPASSES, *PGLASSREFRACTION);
}

float Render::GL::glassDamageRadius(int64_t size, int64_t passes, float refraction) {
    return dualKawaseDamageRadius(size, passes) + std::ceil(std::clamp(refraction, 0.F, MAX_GLASS_REFRACTION));
}
