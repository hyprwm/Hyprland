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

std::string CShaderLoader::getDefines(ShaderFeatureFlags features) {
    std::string                        res     = "";
    std::map<std::string, std::string> defines = {
        {"USE_RGBA", features & SH_FEAT_RGBA ? "1" : "0"},         {"USE_DISCARD", features & SH_FEAT_DISCARD ? "1" : "0"}, {"USE_TINT", features & SH_FEAT_TINT ? "1" : "0"},
        {"USE_ROUNDING", features & SH_FEAT_ROUNDING ? "1" : "0"}, {"USE_CM", features & SH_FEAT_CM ? "1" : "0"},           {"USE_TONEMAP", features & SH_FEAT_TONEMAP ? "1" : "0"},
        {"USE_SDR_MOD", features & SH_FEAT_SDR_MOD ? "1" : "0"},   {"USE_BLUR", features & SH_FEAT_BLUR ? "1" : "0"},       {"USE_ICC", features & SH_FEAT_ICC ? "1" : "0"},
    };
    for (const auto& [name, value] : defines) {
        res += std::format("#define {} {}\n", name, value);
    }
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
        Log::logger->log(Log::ERR, "GLSL preprocessing failed");
        Log::logger->log(Log::ERR, "{}", glslang_shader_get_info_log(shader));
        Log::logger->log(Log::ERR, "{}", glslang_shader_get_info_debug_log(shader));
        Log::logger->log(Log::ERR, "{}", input.code);
        glslang_shader_delete(shader);
        return source;
    }

    std::stringstream stream(glslang_shader_get_preprocessed_code(shader));
    std::string       code = "";
    std::string       line;

    while (std::getline(stream, line)) {
        if (!line.starts_with("#line "))
            code += line + "\n";
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

std::string CShaderLoader::getVariantSource(ePreparedFragmentShader frag, ShaderFeatureFlags features) {
    static const auto PCM = CConfigValue<Hyprlang::INT>("render:cm_enabled");
    if (!*PCM)
        features &= ~(SH_FEAT_CM | SH_FEAT_TONEMAP | SH_FEAT_SDR_MOD);

    if (!m_fragVariants[frag].contains(features)) {
        ASSERT(m_fragFiles[frag].length());
        m_overrideDefines              = getDefines(features);
        m_fragVariants[frag][features] = processSource(m_fragFiles[frag]);
        m_overrideDefines              = "";
    }

    return m_fragVariants[frag][features];
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
        const auto src = NFsUtils::readFileAsString(home.value() + "/hypr/shaders/" + filename);
        if (src.has_value())
            return src.value();
    }
    for (auto& e : ASSET_PATHS) {
        const auto src = NFsUtils::readFileAsString(std::string{e} + "/hypr/shaders/" + filename);
        if (src.has_value())
            return src.value();
    }
    if (SHADERS.contains(filename))
        return SHADERS.at(filename);
    throw std::runtime_error(std::format("Couldn't load shader {}", filename));
}
