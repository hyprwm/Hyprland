#pragma once

#include <array>
#include <glslang/Include/glslang_c_interface.h>
#include <string>
#include <vector>
#include <map>
#include "../helpers/memory/Memory.hpp"

namespace Render {
    enum ePreparedFragmentShaderFeature : uint16_t {
        SH_FEAT_UNKNOWN = 0, // all features just in case

        SH_FEAT_RGBA     = (1 << 0), // RGBA/RGBX texture sampling
        SH_FEAT_DISCARD  = (1 << 1), // RGBA/RGBX texture sampling
        SH_FEAT_TINT     = (1 << 2), // uniforms: tint; condition: applyTint
        SH_FEAT_ROUNDING = (1 << 3), // uniforms: radius, roundingPower, topLeft, fullSize; condition: radius > 0
        SH_FEAT_CM       = (1 << 4), // uniforms: srcTFRange, dstTFRange, srcRefLuminance, convertMatrix; condition: !skipCM
        SH_FEAT_TONEMAP  = (1 << 5), // uniforms: maxLuminance, dstMaxLuminance, dstRefLuminance; condition: maxLuminance < dstMaxLuminance * 1.01
        SH_FEAT_SDR_MOD  = (1 << 6), // uniforms: sdrSaturation, sdrBrightnessMultiplier; condition: SDR <-> HDR && (sdrSaturation != 1 || sdrBrightnessMultiplier != 1)
        SH_FEAT_BLUR     = (1 << 7), // condition: render:use_shader_blur_blend
        SH_FEAT_ICC      = (1 << 8), //

        // uniforms: targetPrimariesXYZ; condition: SH_FEAT_TONEMAP || SH_FEAT_SDR_MOD
    };

    using ShaderFeatureFlags = uint16_t;

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
        SH_FRAG_SURFACE,
        SH_FRAG_BORDER1,
        SH_FRAG_GLITCH,

        SH_FRAG_LAST,
    };

    class CShaderLoader {
      public:
        CShaderLoader(const std::vector<std::string> includes, const std::array<std::string, SH_FRAG_LAST>& frags, const std::string shaderPath = "");
        ~CShaderLoader();

        void                                      include(const std::string& filename);
        std::string                               process(const std::string& filename);
        std::string                               process(const std::string& filename, const std::map<std::string, std::string>& defines);

        std::string                               getVariantSource(ePreparedFragmentShader frag, ShaderFeatureFlags features);

        const std::map<std::string, std::string>& includes();

        std::vector<glsl_include_result_t*>       m_includeResults;

      private:
        std::string loadShader(const std::string& filename);
        std::string getDefines(ShaderFeatureFlags features);
        std::string processSource(const std::string& source, glslang_stage_t stage = GLSLANG_STAGE_FRAGMENT);

        //
        std::string                                                         m_shaderPath;
        std::array<std::string, SH_FRAG_LAST>                               m_fragFiles;
        std::array<std::map<ShaderFeatureFlags, std::string>, SH_FRAG_LAST> m_fragVariants;
        std::map<std::string, std::string>                                  m_includes;

        std::string                                                         m_overrideDefines;
        glsl_include_callbacks_t                                            m_callbacks;
    };

    inline UP<CShaderLoader> g_pShaderLoader;
}
