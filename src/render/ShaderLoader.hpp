#pragma once

#include <array>
#include <compare>
#include <glslang/Include/glslang_c_interface.h>
#include <string>
#include <vector>
#include <map>
#include "../helpers/memory/Memory.hpp"
#include "../helpers/cm/ColorManagement.hpp"

namespace Render {
    enum ePreparedFragmentShaderFeature : uint16_t {
        SH_FEAT_UNKNOWN = 0, // all features just in case

        SH_FEAT_RGBA            = (1 << 0),  // RGBA/RGBX texture sampling
        SH_FEAT_DISCARD         = (1 << 1),  // RGBA/RGBX texture sampling
        SH_FEAT_TINT            = (1 << 2),  // uniforms: tint; condition: applyTint
        SH_FEAT_ROUNDING        = (1 << 3),  // uniforms: radius, roundingPower, topLeft, fullSize; condition: radius > 0
        SH_FEAT_CM              = (1 << 4),  // uniforms: srcTFRange, dstTFRange, srcRefLuminance, convertMatrix; condition: !skipCM
        SH_FEAT_TONEMAP         = (1 << 5),  // uniforms: maxLuminance, dstMaxLuminance, dstRefLuminance; condition: maxLuminance < dstMaxLuminance * 1.01
        SH_FEAT_SDR_MOD         = (1 << 6),  // uniforms: sdrSaturation, sdrBrightnessMultiplier; condition: SDR <-> HDR && (sdrSaturation != 1 || sdrBrightnessMultiplier != 1)
        SH_FEAT_BLUR            = (1 << 7),  // condition: render:use_shader_blur_blend
        SH_FEAT_ICC             = (1 << 8),  //
        SH_FEAT_MIRROR          = (1 << 9),  // condition: mirror or screenshare
        SH_FEAT_MOTION_BLUR     = (1 << 10), // condition: decoration:motion_blur:enabled
        SH_FEAT_BLUR_ALPHA_MASK = (1 << 11), // condition: transformed-window shader blur blend
        SH_FEAT_BLUR_MATTE      = (1 << 12), // condition: transformed-window blur matte
        SH_FEAT_ALT_TONEMAP     = (1 << 13), // condition: tonemapMode == 3

        // uniforms: targetPrimariesXYZ; condition: SH_FEAT_TONEMAP || SH_FEAT_SDR_MOD
    };

    using ShaderFeatureFlags = uint16_t;

    constexpr NColorManagement::eTransferFunction SHADER_DEFAULT_TF = NColorManagement::CM_TRANSFER_FUNCTION_SRGB;
    struct SShaderVariant {
        ShaderFeatureFlags                  features = 0;
        NColorManagement::eTransferFunction sourceTF = SHADER_DEFAULT_TF;
        NColorManagement::eTransferFunction targetTF = SHADER_DEFAULT_TF;

        auto                                operator<=>(const SShaderVariant&) const = default;
    };

    enum ePreparedFragmentShader : uint8_t {
        SH_FRAG_QUAD = 0,
        SH_FRAG_PASSTHRURGBA,
        SH_FRAG_MATTE,
        SH_FRAG_EXT,
        SH_FRAG_BLUR1,
        SH_FRAG_BLUR2,
        SH_FRAG_BLURPREPARE,
        SH_FRAG_BLURFINISH,
        SH_FRAG_SHADOW,
        SH_FRAG_INNER_GLOW,
        SH_FRAG_SURFACE,
        SH_FRAG_BORDER1,
        SH_FRAG_GLITCH,
        SH_FRAG_FROSTFINISH,
        SH_FRAG_RIPPLEFINISH,
        SH_FRAG_DROPSFINISH,
        SH_FRAG_WATERSTEP,
        SH_FRAG_WATERFINISH,
        SH_FRAG_FLUIDJARINIT,
        SH_FRAG_FLUIDJARSTEP,
        SH_FRAG_FLUIDJARGRAPH,
        SH_FRAG_FLUIDJARTRACK,
        SH_FRAG_FLUIDJARVISUAL,
        SH_FRAG_FLUIDJARRESAMPLE,
        SH_FRAG_FLUIDJARHISTORYRESAMPLE,
        SH_FRAG_FLUIDJARTRACKINGRESAMPLE,
        SH_FRAG_FLUIDJARFINISH,
        SH_FRAG_PRISMFINISH,
        SH_FRAG_HEATSHIMMERFINISH,
        SH_FRAG_ACRYLICFINISH,
        SH_FRAG_AURORAFINISH,

        SH_FRAG_LAST,
    };

    class CShaderLoader {
      public:
        CShaderLoader(const std::vector<std::string> includes, const std::array<std::string, SH_FRAG_LAST>& frags, const std::string shaderPath = "");
        ~CShaderLoader();

        void                                      include(const std::string& filename);
        std::string                               process(const std::string& filename);
        std::string                               process(const std::string& filename, const std::map<std::string, std::string>& defines);

        std::string                               getVariantSource(ePreparedFragmentShader frag, SShaderVariant variant);

        const std::map<std::string, std::string>& includes();

        std::vector<glsl_include_result_t*>       m_includeResults;

      private:
        std::string loadShader(const std::string& filename);
        std::string getDefines(const SShaderVariant& variant);
        std::string processSource(const std::string& source, glslang_stage_t stage = GLSLANG_STAGE_FRAGMENT);

        //
        std::string                                                     m_shaderPath;
        std::array<std::string, SH_FRAG_LAST>                           m_fragFiles;
        std::array<std::map<SShaderVariant, std::string>, SH_FRAG_LAST> m_fragVariants;
        std::map<std::string, std::string>                              m_includes;

        std::string                                                     m_overrideDefines;
        glsl_include_callbacks_t                                        m_callbacks;
    };

    inline UP<CShaderLoader> g_pShaderLoader;
}
