#include <render/shaders/Shaders.hpp>
#include <helpers/memory/Memory.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

#include <gtest/gtest.h>
#include <deque>
#include <format>
#include <string>

class CRoundingShaders : public testing::Test {
  protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(glslang_initialize_process());
    }

    static void TearDownTestSuite() {
        glslang_finalize_process();
    }

    void compile(std::string_view filename, const std::string& defines) {
        const auto SOURCE = std::ranges::find(SHADERS, filename, &std::pair<std::string_view, std::string_view>::first);
        ASSERT_NE(SOURCE, SHADERS.end());

        auto allDefines = defines;
        for (const auto* feature : {"USE_RGBA", "USE_DISCARD", "USE_TINT", "USE_ROUNDING", "USE_CM", "USE_TONEMAP", "USE_ALT_TONEMAP", "USE_SDR_MOD", "USE_BLUR", "USE_ICC",
                                    "USE_MIRROR", "USE_MOTION_BLUR", "USE_BLUR_ALPHA_MASK", "USE_BLUR_MATTE", "USE_BLUR_PROPORTIONAL"})
            allDefines += std::format("#ifndef {}\n#define {} 0\n#endif\n", feature, feature);

        struct SIncludes {
            const std::string&                defines;
            std::deque<glsl_include_result_t> results;
        } includes{allDefines, {}};

        const glslang_input_t INPUT{
            .language                          = GLSLANG_SOURCE_GLSL,
            .stage                             = GLSLANG_STAGE_FRAGMENT,
            .client                            = GLSLANG_CLIENT_NONE,
            .target_language                   = GLSLANG_TARGET_NONE,
            .code                              = SOURCE->second.data(),
            .default_version                   = 100,
            .default_profile                   = GLSLANG_NO_PROFILE,
            .force_default_version_and_profile = false,
            .forward_compatible                = false,
            .messages                          = GLSLANG_MSG_DEFAULT_BIT,
            .resource                          = glslang_default_resource(),
            .callbacks                         = {
                .include_local = [](void* context, const char* name, const char*, size_t) -> glsl_include_result_t* {
                    auto&      ctx    = *sc<SIncludes*>(context);
                    const auto SOURCE = std::ranges::find(SHADERS, std::string_view{name}, &std::pair<std::string_view, std::string_view>::first);
                    if (SOURCE == SHADERS.end())
                        return nullptr;

                    const std::string_view TEXT = std::string_view{name} == "defines.h" ? ctx.defines : SOURCE->second;
                    ctx.results.push_back({SOURCE->first.data(), TEXT.data(), TEXT.size()});
                    return &ctx.results.back();
                },
                .free_include_result = [](void*, glsl_include_result_t*) { return 0; },
            },
            .callbacks_ctx = &includes,
        };

        auto shader = glslang_shader_create(&INPUT);
        ASSERT_NE(shader, nullptr);
        const Hyprutils::Utils::CScopeGuard CLEANUP{[shader] { glslang_shader_delete(shader); }};
        ASSERT_TRUE(glslang_shader_preprocess(shader, &INPUT)) << filename << '\n' << defines << glslang_shader_get_info_log(shader);
        ASSERT_TRUE(glslang_shader_parse(shader, &INPUT)) << filename << '\n' << defines << glslang_shader_get_info_log(shader);
    }
};

TEST_F(CRoundingShaders, RoundedDrawVariantsCompileWithoutGpu) {
    for (const auto* shader : {"quad.frag", "surface.frag", "ext.frag", "border.frag"}) {
        for (int rounding = 0; rounding <= 1; ++rounding) {
            for (int mirror = 0; mirror <= 1; ++mirror) {
                for (int cm = 0; cm <= 1; ++cm) {
                    const auto DEFINES = std::format("#define USE_ROUNDING {}\n#define USE_MIRROR {}\n#define USE_CM {}\n#define USE_ICC {}\n"
                                                     "#define USE_RGBA 1\n#define USE_DISCARD 1\n#define USE_TINT 1\n#define SOURCE_TF 9\n#define TARGET_TF 9\n",
                                                     rounding, mirror, cm, 0);
                    compile(shader, DEFINES);
                }
            }
        }
    }
}

TEST_F(CRoundingShaders, SurfaceIccVariantsCompileWithoutGpu) {
    for (int rounding = 0; rounding <= 1; ++rounding)
        compile("surface.frag",
                std::format("#define USE_ROUNDING {}\n#define USE_RGBA 1\n#define USE_CM 1\n#define USE_ICC 1\n"
                            "#define SOURCE_TF 9\n#define TARGET_TF 9\n",
                            rounding));
}

TEST_F(CRoundingShaders, BorderIccVariantsCompileWithoutGpu) {
    for (int rounding = 0; rounding <= 1; ++rounding)
        compile("border.frag", std::format("#define USE_ROUNDING {}\n#define USE_CM 1\n#define USE_ICC 1\n#define SOURCE_TF 9\n#define TARGET_TF 9\n", rounding));
}

TEST_F(CRoundingShaders, AcrylicFinishVariantsCompileWithoutGpu) {
    for (int cm = 0; cm <= 1; ++cm)
        compile("acrylicfinish.frag", std::format("#define USE_CM {}\n#define SOURCE_TF 9\n#define TARGET_TF 9\n", cm));
}

TEST_F(CRoundingShaders, SurfaceBlurAndMotionVariantsCompileWithoutGpu) {
    for (int motion = 0; motion <= 1; ++motion) {
        for (int blurMode = 0; blurMode <= 3; ++blurMode) {
            compile("surface.frag",
                    std::format("#define USE_ROUNDING 1\n#define USE_RGBA 1\n#define USE_MIRROR {}\n#define USE_CM 1\n"
                                "#define USE_TONEMAP 1\n#define USE_SDR_MOD 1\n#define USE_BLUR {}\n#define USE_BLUR_MATTE {}\n"
                                "#define USE_BLUR_ALPHA_MASK {}\n#define USE_BLUR_PROPORTIONAL {}\n#define USE_MOTION_BLUR {}\n"
                                "#define SOURCE_TF 9\n#define TARGET_TF 9\n",
                                blurMode == 3 ? 0 : 1, blurMode != 0 ? 1 : 0, blurMode == 2 ? 1 : 0, blurMode == 2 ? 1 : 0, blurMode == 3 ? 1 : 0, motion));
        }
    }
}
