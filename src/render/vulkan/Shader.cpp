#include "Shader.hpp"
#include "macros.hpp"
#include "utils.hpp"

// based on https://github.com/KhronosGroup/glslang?tab=readme-ov-file#c-functional-interface-new
#include <cstdint>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

#define CLIENT_VERSION          GLSLANG_TARGET_VULKAN_1_4
#define TARGET_LANGUAGE_VERSION GLSLANG_TARGET_SPV_1_6

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
        // printf("GLSL preprocessing failed %s\n", fileName);
        // printf("%s\n", glslang_shader_get_info_log(shader));
        // printf("%s\n", glslang_shader_get_info_debug_log(shader));
        // printf("%s\n", input.code);
        glslang_shader_delete(shader);
        return {};
    }

    if (!glslang_shader_parse(shader, &input)) {
        // printf("GLSL parsing failed %s\n", fileName);
        // printf("%s\n", glslang_shader_get_info_log(shader));
        // printf("%s\n", glslang_shader_get_info_debug_log(shader));
        // printf("%s\n", glslang_shader_get_preprocessed_code(shader));
        glslang_shader_delete(shader);
        return {};
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        // printf("GLSL linking failed %s\n", fileName);
        // printf("%s\n", glslang_program_get_info_log(program));
        // printf("%s\n", glslang_program_get_info_debug_log(program));
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return {};
    }

    glslang_program_SPIRV_generate(program, stage);

    const auto            size = glslang_program_SPIRV_get_size(program);
    std::vector<uint32_t> binary(size);
    glslang_program_SPIRV_get(program, binary.data());

    // const char* spirv_messages = glslang_program_SPIRV_get_messages(program);
    // if (spirv_messages)
    //     printf("(%s) %s\b", fileName, spirv_messages);

    glslang_program_delete(program);
    glslang_shader_delete(shader);

    return binary;
}

CVkShader::CVkShader(WP<CHyprVulkanDevice> device, const std::string& source, eShaderType type) : IDeviceUser(device) {
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
}

VkShaderModule CVkShader::module() {
    return m_module;
}

CVkShader::~CVkShader() {
    if (m_module)
        vkDestroyShaderModule(vkDevice(), m_module, nullptr);
}