#include "Shaders.hpp"
#include "Shader.hpp"
#include "../ShaderLoader.hpp"
#include "../../macros.hpp"
#include "types.hpp"
#include <hyprutils/memory/UniquePtr.hpp>
#include "../../debug/log/Logger.hpp"

using namespace Render;
using namespace Render::VK;

static const std::vector<std::string> SHADER_INCLUDES = {
    "defines.h", "constants.h", "structs.h",   "cm_helpers.glsl",  "rounding.glsl", "CM.glsl",    "tonemap.glsl",
    "gain.glsl", "border.glsl", "shadow.glsl", "blurprepare.glsl", "blur1.glsl",    "blur2.glsl", "blurFinish.glsl",
};

// TODO vulkan ext.frag, glitch.frag, inner_glow.frag
// order matters, see ePreparedFragmentShader
const std::array<std::string, SH_FRAG_LAST> FRAG_SHADERS = {
    "vk_quad.frag",       "vk_pass.frag",   "vk_matte.frag",   "ext.frag",    "vk_blur1.frag",  "vk_blur2.frag", "vk_blurprepare.frag",
    "vk_blurfinish.frag", "vk_shadow.frag", "inner_glow.frag", "vk_tex.frag", "vk_border.frag", "glitch.frag",
};

#define VK_PUSH_MIN_SIZE 128

CVkShaders::CVkShaders(WP<CHyprVulkanDevice> device, const std::string& shaderPath) : IDeviceUser(device) {
    if (!g_pShaderLoader)
        g_pShaderLoader = makeUnique<CShaderLoader>(SHADER_INCLUDES, FRAG_SHADERS, shaderPath);

    if (sizeof(SVkVertShaderData) + sizeof(SVkFragShaderData) > VK_PUSH_MIN_SIZE)
        Log::logger->log(Log::WARN, "Main texture shader push constants size ({}) is higher than the guaranteed size ({})", sizeof(SVkVertShaderData) + sizeof(SVkFragShaderData),
                         VK_PUSH_MIN_SIZE);

    m_vert = makeShared<CVkShader>(device, g_pShaderLoader->process("vulkan.vert"), sizeof(SVkVertShaderData), SH_VERT, "vert");
}

static int getShaderDataSize(ePreparedFragmentShader frag, uint8_t features) {
    switch (frag) {
        case SH_FRAG_QUAD: return sizeof(SVkRectShaderData) + (features & SH_FEAT_ROUNDING ? sizeof(SRounding) : 0);
        case SH_FRAG_PASSTHRURGBA: return 0;
        case SH_FRAG_MATTE: return 0;
        case SH_FRAG_EXT: return 0; // unsupported
        case SH_FRAG_BLUR1: return sizeof(SVkBlur1ShaderData);
        case SH_FRAG_BLUR2: return sizeof(SVkBlur2ShaderData);
        case SH_FRAG_BLURPREPARE: return sizeof(SVkPrepareShaderData) + (features & SH_FEAT_CM ? sizeof(SShaderCM) : 0);
        case SH_FRAG_BLURFINISH: return sizeof(SVkFinishShaderData);
        case SH_FRAG_SHADOW:
            return sizeof(SVkShadowShaderData) + (features & SH_FEAT_ROUNDING ? sizeof(SRounding) : 0) + (features & SH_FEAT_CM ? sizeof(SShaderCM) : 0) +
                (features & SH_FEAT_TONEMAP ? sizeof(SShaderTonemap) : 0) + (features & SH_FEAT_TONEMAP || features & SH_FEAT_SDR_MOD ? sizeof(SShaderTargetPrimaries) : 0);
        case SH_FRAG_SURFACE:
            return sizeof(SVkFragShaderData) + (features & SH_FEAT_ROUNDING ? sizeof(SRounding) : 0) + (features & SH_FEAT_CM ? sizeof(SShaderCM) : 0) +
                (features & SH_FEAT_TONEMAP ? sizeof(SShaderTonemap) : 0) + (features & SH_FEAT_TONEMAP || features & SH_FEAT_SDR_MOD ? sizeof(SShaderTargetPrimaries) : 0);
        case SH_FRAG_BORDER1:
            return sizeof(SVkBorderShaderData) + (features & SH_FEAT_ROUNDING ? sizeof(SRounding) : 0) + (features & SH_FEAT_CM ? sizeof(SShaderCM) : 0) +
                (features & SH_FEAT_TONEMAP ? sizeof(SShaderTonemap) : 0) + (features & SH_FEAT_TONEMAP || features & SH_FEAT_SDR_MOD ? sizeof(SShaderTargetPrimaries) : 0);
        case SH_FRAG_GLITCH: return 0; // unsupported
        default: UNREACHABLE();
    }
}

WP<CVkShader> CVkShaders::getShaderVariant(ePreparedFragmentShader frag, uint8_t features) {
    if (!m_fragVariants[frag].contains(features)) {
        Log::logger->log(Log::INFO, "compiling feature set {} for {}", features, FRAG_SHADERS[frag]);

        auto shader = makeShared<CVkShader>(m_device, g_pShaderLoader->getVariantSource(frag, features), getShaderDataSize(frag, features), SH_FRAG,
                                            std::format("{}({})", FRAG_SHADERS[frag], features));

        if (!shader)
            Log::logger->log(Log::ERR, "shader features {} failed for {}", features, FRAG_SHADERS[frag]);

        m_fragVariants[frag][features] = shader;
        return shader;
    }

    ASSERT(m_fragVariants[frag][features]);
    return m_fragVariants[frag][features];
}
