#include "Glass.hpp"

#include "Kawase.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace Render;
using namespace Render::GL;

static constexpr float MAX_GLASS_REFRACTION = 20.F;

CGlassBlurMaterial::CGlassBlurMaterial(eBlurType type, ePreparedFragmentShader finishFragment, bool preparedInput) :
    m_type(type), m_finishFragment(finishFragment), m_preparedInput(preparedInput) {
    ;
}

CGlassBlurProvider::CGlassBlurProvider(CHyprOpenGLImpl& impl, eBlurType type, ePreparedFragmentShader finishFragment) :
    CDualKawaseBlurProvider(impl, makeUnique<CGlassBlurMaterial>(type, finishFragment)) {
    ;
}

CGlassBlurProvider::CGlassBlurProvider(CHyprOpenGLImpl& impl, UP<IGLBlurMaterial> material) : CDualKawaseBlurProvider(impl, std::move(material)) {
    ;
}

eBlurType CGlassBlurMaterial::type() const noexcept {
    return m_type;
}

SBlurMaterialRequirements CGlassBlurMaterial::requirements() const noexcept {
    return {
        .finishFragment = m_finishFragment,
        .preparedInput  = m_preparedInput,
    };
}

void CGlassBlurMaterial::bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const {
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");
    static auto PGLASSSIZE       = CConfigValue<Config::FLOAT>("decoration:blur:glass:size");
    static auto PGLASSROUGHNESS  = CConfigValue<Config::FLOAT>("decoration:blur:glass:roughness");

    const auto  clampedStrength = std::clamp(context.strength, 0.F, 1.F);

    shader->setUniformFloat(SHADER_GLASS_REFRACTION, std::clamp(*PGLASSREFRACTION, 0.F, MAX_GLASS_REFRACTION) * clampedStrength);
    shader->setUniformFloat(SHADER_GLASS_SIZE, std::clamp(*PGLASSSIZE, 4.F, 512.F));
    shader->setUniformFloat(SHADER_GLASS_ROUGHNESS, std::clamp(*PGLASSROUGHNESS, 0.F, 1.F) * clampedStrength);

    CBox transformedPatternBox{};
    if (context.blurContext.patternBox && g_pHyprRenderer->m_renderData.pMonitor) {
        const auto MONITOR    = g_pHyprRenderer->m_renderData.pMonitor;
        transformedPatternBox = *context.blurContext.patternBox;
        transformedPatternBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(MONITOR->m_transform)), MONITOR->m_transformedSize.x, MONITOR->m_transformedSize.y);
    }

    shader->setUniformFloat2(SHADER_GLASS_POSITION, sc<float>(transformedPatternBox.x), sc<float>(transformedPatternBox.y));
}

float CGlassBlurMaterial::sampleRadius() const {
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");

    return std::ceil(std::clamp(*PGLASSREFRACTION, 0.F, MAX_GLASS_REFRACTION));
}

float Render::GL::glassDamageRadius(int64_t size, int64_t passes, float refraction) {
    return dualKawaseDamageRadius(size, passes) + std::ceil(std::clamp(refraction, 0.F, MAX_GLASS_REFRACTION));
}
