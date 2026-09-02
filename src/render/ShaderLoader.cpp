#include "ShaderLoader.hpp"
#include <format>
#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/path/Path.hpp>
#include "../debug/log/Logger.hpp"
#include "shaders/Shaders.hpp"
#include "../helpers/fs/FsUtils.hpp"
#include "Renderer.hpp"
#include <glslang/Public/resource_limits_c.h>
#include <string>
#include <filesystem>

using namespace Render;

CShaderLoader::CShaderLoader(const std::vector<std::string> includes, const std::array<std::string, SH_FRAG_LAST>& frags, const std::string shaderPath) : m_shaderPath(shaderPath) {
    m_callbacks = glsl_include_callbacks_t{
        .include_local =
            [](void* ctx, const char* header_name, const char* includer_name, size_t include_depth) {
                auto shaderLoader = sc<CShaderLoader*>(ctx);
                auto res          = new glsl_include_result_t;
                if (shaderLoader->m_overrideDefines.length() && std::string{header_name} == "defines.h") {
                    res->header_name   = header_name;
                    res->header_data   = shaderLoader->m_overrideDefines.c_str();
                    res->header_length = shaderLoader->m_overrideDefines.length();
                } else if (shaderLoader->includes().contains(header_name)) {
                    res->header_name   = header_name;
                    res->header_data   = shaderLoader->includes().at(header_name).c_str();
                    res->header_length = shaderLoader->includes().at(header_name).length();
                } else {
                    res->header_name   = nullptr;
                    res->header_data   = nullptr;
                    res->header_length = 0;
                }

                shaderLoader->m_includeResults.push_back(res);
                return res;
            },
        .free_include_result =
            [](void* ctx, glsl_include_result_t* result) {
                auto shaderLoader = sc<CShaderLoader*>(ctx);
                std::erase(shaderLoader->m_includeResults, result);
                delete result;
                return 0;
            },
    };

    for (const auto& inc : includes) {
        include(inc);
    }

    std::ranges::transform(frags, m_fragFiles.begin(), [&](const auto& filename) { return loadShader(filename); });
}

CShaderLoader::~CShaderLoader() {
    // glslFreeIncludeResult should leave it empty by this point
    for (const auto& res : m_includeResults) {
        delete res;
    }
}

void CShaderLoader::include(const std::string& filename) {
    m_includes.insert({filename, loadShader(filename)});
}

std::string CShaderLoader::getDefines(const SShaderVariant& variant) {
    static constexpr auto defines = std::to_array<std::pair<std::string_view, ePreparedFragmentShaderFeature>>({
        {"USE_RGBA", SH_FEAT_RGBA},
        {"USE_DISCARD", SH_FEAT_DISCARD},
        {"USE_TINT", SH_FEAT_TINT},
        {"USE_ROUNDING", SH_FEAT_ROUNDING},
        {"USE_CM", SH_FEAT_CM},
        {"USE_TONEMAP", SH_FEAT_TONEMAP},
        {"USE_SDR_MOD", SH_FEAT_SDR_MOD},
        {"USE_BLUR", SH_FEAT_BLUR},
        {"USE_ICC", SH_FEAT_ICC},
        {"USE_MIRROR", SH_FEAT_MIRROR},
        {"USE_MOTION_BLUR", SH_FEAT_MOTION_BLUR},
        {"USE_BLUR_ALPHA_MASK", SH_FEAT_BLUR_ALPHA_MASK},
        {"USE_BLUR_MATTE", SH_FEAT_BLUR_MATTE},
        {"USE_ALT_TONEMAP", SH_FEAT_ALT_TONEMAP},
    });

    std::string           res;
    res.reserve(351);
    for (const auto& [name, flag] : defines) {
        std::format_to(std::back_inserter(res), "#define {} {}\n", name, (variant.features & flag) != 0 ? '1' : '0');
    }

    // eTransferFunction values, the shaders compare them against the CM_TRANSFER_FUNCTION_* in CM.glsl
    std::format_to(std::back_inserter(res), "#define SOURCE_TF {}\n", sc<int>(variant.sourceTF));
    std::format_to(std::back_inserter(res), "#define TARGET_TF {}\n", sc<int>(variant.targetTF));
    return res;
}

std::string CShaderLoader::processSource(const std::string& source, glslang_stage_t stage) {
    const glslang_input_t input = {
        .language                          = GLSLANG_SOURCE_GLSL,
        .stage                             = stage,
        .client                            = GLSLANG_CLIENT_NONE,
        .target_language                   = GLSLANG_TARGET_NONE,
        .code                              = source.c_str(),
        .default_version                   = 100,
        .default_profile                   = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible                = false,
        .messages                          = GLSLANG_MSG_DEFAULT_BIT,
        .resource                          = glslang_default_resource(),
        .callbacks                         = m_callbacks,
        .callbacks_ctx                     = this,
    };

    glslang_shader_t* shader = glslang_shader_create(&input);

    if (!glslang_shader_preprocess(shader, &input)) {
        LOG(Log::ERR, "GLSL preprocessing failed");
        LOG(Log::ERR, "{}", glslang_shader_get_info_log(shader));
        LOG(Log::ERR, "{}", glslang_shader_get_info_debug_log(shader));
        LOG(Log::ERR, "{}", input.code);
        glslang_shader_delete(shader);
        return source;
    }

    std::stringstream stream(glslang_shader_get_preprocessed_code(shader));
    std::string       code = "";
    std::string       line;

    while (std::getline(stream, line)) {
        if (!line.starts_with("#line "))
            code += std::format("{}\n", line);
    }

    glslang_shader_delete(shader);
    return code;
}

std::string CShaderLoader::process(const std::string& filename) {
    auto source = loadShader(filename);
    return processSource(source, filename.ends_with(".vert") ? GLSLANG_STAGE_VERTEX : GLSLANG_STAGE_FRAGMENT);
}

std::string CShaderLoader::process(const std::string& filename, const std::map<std::string, std::string>& defines) {
    m_overrideDefines = "";
    for (const auto& [name, value] : defines) {
        m_overrideDefines += std::format("#define {} {}\n", name, value);
    }
    const auto& res   = process(filename);
    m_overrideDefines = "";
    return res;
}

std::string CShaderLoader::getVariantSource(ePreparedFragmentShader frag, SShaderVariant variant) {
    static const auto PCM = CConfigValue<Config::INTEGER>("render:cm_enabled");
    if (!*PCM)
        variant.features &= ~(SH_FEAT_CM | SH_FEAT_TONEMAP | SH_FEAT_ALT_TONEMAP | SH_FEAT_SDR_MOD);

    // without CM the transfer functions are unused, keep them at the default so we don't cache
    // several variants of identical source
    if (!(variant.features & SH_FEAT_CM)) {
        variant.sourceTF = SHADER_DEFAULT_TF;
        variant.targetTF = SHADER_DEFAULT_TF;
    }

    if (!m_fragVariants[frag].contains(variant)) {
        ASSERT(m_fragFiles[frag].length());
        m_overrideDefines             = getDefines(variant);
        m_fragVariants[frag][variant] = processSource(m_fragFiles[frag]);
        m_overrideDefines             = "";
    }

    return m_fragVariants[frag][variant];
}

const std::map<std::string, std::string>& CShaderLoader::includes() {
    return m_includes;
}

// TODO notify user if bundled shader is newer than ~/.config override
std::string CShaderLoader::loadShader(const std::string& filename) {
    if (m_shaderPath.length()) {
        std::filesystem::path path = m_shaderPath;
        const auto            src  = NFsUtils::readFileAsString(path / filename);
        if (src.has_value())
            return src.value();
    }
    const auto home = Hyprutils::Path::getHome();
    if (home.has_value()) {
        const auto src = NFsUtils::readFileAsString(std::format("{}/hypr/shaders/{}", home.value(), filename));
        if (src.has_value())
            return src.value();
    }
    for (auto& e : ASSET_PATHS) {
        const auto src = NFsUtils::readFileAsString(std::format("{}/hypr/shaders/{}", e, filename));
        if (src.has_value())
            return src.value();
    }

    const auto shader = std::ranges::lower_bound(SHADERS, filename, {}, [](const auto& filenameSource) { return filenameSource.first; });
    if (shader != SHADERS.end() && shader->first == filename)
        return std::string{shader->second};
    throw std::runtime_error(std::format("Couldn't load shader {}", filename));
}
