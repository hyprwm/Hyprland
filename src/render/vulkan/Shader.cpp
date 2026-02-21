#include "Shader.hpp"
#include "debug/log/Logger.hpp"
#include "macros.hpp"
#include "render/vulkan/Vulkan.hpp"
#include "utils.hpp"

// based on https://github.com/KhronosGroup/glslang?tab=readme-ov-file#c-functional-interface-new
#include <cmath>
#include <cstdint>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

// GLSLANG_TARGET_SPV_1_5 for shader "discard" op
#define CLIENT_VERSION          GLSLANG_TARGET_VULKAN_1_4
#define TARGET_LANGUAGE_VERSION GLSLANG_TARGET_SPV_1_5

static std::optional<std::vector<uint32_t>> compileShader(glslang_stage_t stage, const std::string& shaderSource) {
    const glslang_input_t input = {
        .language                          = GLSLANG_SOURCE_GLSL,
        .stage                             = stage,
        .client                            = GLSLANG_CLIENT_VULKAN,
        .client_version                    = CLIENT_VERSION,
        .target_language                   = GLSLANG_TARGET_SPV,
        .target_language_version           = TARGET_LANGUAGE_VERSION,
        .code                              = shaderSource.c_str(),
        .default_version                   = 100,
        .default_profile                   = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible                = false,
        .messages                          = GLSLANG_MSG_DEFAULT_BIT,
        .resource                          = glslang_default_resource(),
    };

    glslang_shader_t* shader = glslang_shader_create(&input);

    if (!glslang_shader_preprocess(shader, &input)) {
        Log::logger->log(Log::ERR, "GLSL preprocessing failed");
        Log::logger->log(Log::ERR, "{}", glslang_shader_get_info_log(shader));
        Log::logger->log(Log::ERR, "{}", glslang_shader_get_info_debug_log(shader));
        Log::logger->log(Log::ERR, "{}", input.code);
        glslang_shader_delete(shader);
        return {};
    }

    if (!glslang_shader_parse(shader, &input)) {
        Log::logger->log(Log::ERR, "GLSL parsing failed");
        Log::logger->log(Log::ERR, "{}", glslang_shader_get_info_log(shader));
        Log::logger->log(Log::ERR, "{}", glslang_shader_get_info_debug_log(shader));
        Log::logger->log(Log::ERR, "{}", glslang_shader_get_preprocessed_code(shader));
        glslang_shader_delete(shader);
        return {};
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        Log::logger->log(Log::ERR, "GLSL linking failed");
        Log::logger->log(Log::ERR, "{}", glslang_program_get_info_log(program));
        Log::logger->log(Log::ERR, "{}", glslang_program_get_info_debug_log(program));
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return {};
    }

    glslang_program_SPIRV_generate(program, stage);

    const auto            size = glslang_program_SPIRV_get_size(program);
    std::vector<uint32_t> binary(size);
    glslang_program_SPIRV_get(program, binary.data());

    const char* spirv_messages = glslang_program_SPIRV_get_messages(program);
    if (spirv_messages)
        Log::logger->log(Log::DEBUG, "{}", spirv_messages);

    glslang_program_delete(program);
    glslang_shader_delete(shader);

    return binary;
}

CVkShader::CVkShader(WP<CHyprVulkanDevice> device, const std::string& source, uint32_t pushSize, eShaderType type, const std::string& name) :
    IDeviceUser(device), m_name(name), m_pushSize(std::ceil(pushSize / 16.f) * 16) {
    const auto binary = compileShader(type == SH_FRAG ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_VERTEX, source);

    ASSERT(binary.has_value());

    VkShaderModuleCreateInfo info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(uint32_t) * binary->size(),
        .pCode    = binary->data(),
    };
    if (vkCreateShaderModule(vkDevice(), &info, nullptr, &m_module) != VK_SUCCESS) {
        CRIT("vkCreateShaderModule");
    }
    if (m_name.length())
        SET_VK_SHADER_NAME(m_module, m_name)
}

VkShaderModule CVkShader::module() {
    return m_module;
}

uint32_t CVkShader::pushSize() {
    return m_pushSize;
}

CVkShader::~CVkShader() {
    if (m_module)
        vkDestroyShaderModule(vkDevice(), m_module, nullptr);
}