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
#include <optional>
#include <sstream>
#include <string>
#include <filesystem>

using namespace Render;

// guards against include cycles, which can't be detected without evaluating #ifndef guards
static constexpr size_t MAX_INCLUDE_DEPTH = 16;

// returns the file of an #include "file" directive
static std::optional<std::string> includedFile(std::string_view line) {
    line = Hyprutils::String::trim(line);
    if (!line.starts_with("#include"))
        return std::nullopt;

    const auto BEGIN = line.find('"');
    if (BEGIN == std::string_view::npos)
        return std::nullopt;

    const auto END = line.find('"', BEGIN + 1);
    if (END == std::string_view::npos)
        return std::nullopt;

    return std::string{line.substr(BEGIN + 1, END - BEGIN - 1)};
}

CShaderLoader::CShaderLoader(const std::vector<std::string> includes, const std::array<std::string, SH_FRAG_LAST>& frags, const std::string shaderPath) : m_shaderPath(shaderPath) {
    for (const auto& inc : includes) {
        include(inc);
    }

    std::ranges::transform(frags, m_fragFiles.begin(), [&](const auto& filename) { return loadShader(filename); });
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
        {"USE_BLUR_PROPORTIONAL", SH_FEAT_BLUR_PROPORTIONAL},
        {"USE_EXTERNAL", SH_FEAT_EXTERNAL},
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

// expands includes in place, everything else (#if etc.) is left to the driver's preprocessor
std::string CShaderLoader::processSource(const std::string& source, size_t depth) {
    std::stringstream stream(source);
    std::string       code;
    std::string       line;

    while (std::getline(stream, line)) {
        const auto INCLUDE = includedFile(line);

        if (!INCLUDE) {
            std::format_to(std::back_inserter(code), "{}\n", line);
            continue;
        }

        if (*INCLUDE == "defines.h" && !m_overrideDefines.empty()) {
            code += m_overrideDefines;
            continue;
        }

        if (depth >= MAX_INCLUDE_DEPTH || !m_includes.contains(*INCLUDE)) {
            // keep the directive, the driver will fail the compile on it
            LOG(Log::ERR, "Shader include \"{}\" is missing or nested too deep", *INCLUDE);
            std::format_to(std::back_inserter(code), "{}\n", line);
            continue;
        }

        code += processSource(m_includes.at(*INCLUDE), depth + 1);
    }

    return code;
}

std::string CShaderLoader::process(const std::string& filename) {
    return processSource(loadShader(filename));
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
    static const auto PCM = CConfigValue<Config::BOOL>("render:cm_enabled");
    if (!*PCM)
        variant.features &= ~(SH_FEAT_CM | SH_FEAT_TONEMAP | SH_FEAT_ALT_TONEMAP | SH_FEAT_SDR_MOD);

    // without CM the transfer functions are unused, keep them at the default so identical
    // shaders end up with identical source
    if (!(variant.features & SH_FEAT_CM)) {
        variant.sourceTF = SHADER_DEFAULT_TF;
        variant.targetTF = SHADER_DEFAULT_TF;
    }

    ASSERT(m_fragFiles[frag].length());

    m_overrideDefines = getDefines(variant);
    auto source       = processSource(m_fragFiles[frag]);
    m_overrideDefines = "";

    return source;
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
