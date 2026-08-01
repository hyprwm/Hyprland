#include "Haze.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../helpers/cm/ColorManagement.hpp"

#include <algorithm>

using namespace Render;
using namespace Render::GL;
using namespace NColorManagement;

eBlurType CHazeBlurMaterial::type() const noexcept {
    return eBlurType::BLUR_HAZE;
}

SBlurMaterialRequirements CHazeBlurMaterial::requirements() const noexcept {
    return {
        .finishFragment = SH_FRAG_HAZEFINISH,
    };
}

int64_t CHazeBlurMaterial::blurSizeForDamage(int64_t size) const {
    return std::clamp<int64_t>(size, 1, 40);
}

void CHazeBlurMaterial::bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const {
    static auto PHAZEINTENSITY   = CConfigValue<Config::FLOAT>("decoration:blur:haze:intensity");
    static auto PHAZEIRIDESCENCE = CConfigValue<Config::FLOAT>("decoration:blur:haze:iridescence");

    shader->setUniformFloat(SHADER_HAZE_INTENSITY, std::clamp(*PHAZEINTENSITY, 0.F, 1.F) * std::clamp(context.strength, 0.F, 1.F));
    shader->setUniformFloat(SHADER_HAZE_IRIDESCENCE, std::clamp(*PHAZEIRIDESCENCE, 0.F, 1.F));
    shader->setUniformInt(SHADER_HAZE_TRANSFER_FUNCTION, sc<int>(getDefaultImageDescription()->value().transferFunction));
}

CHazeBlurProvider::CHazeBlurProvider(CHyprOpenGLImpl& impl) : CDualKawaseBlurProvider(impl, makeUnique<CHazeBlurMaterial>()) {
    ;
}
