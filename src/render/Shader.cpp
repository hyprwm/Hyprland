#include "Shader.hpp"
#include "render/OpenGL.hpp"

#define EPSILON(x, y) (std::abs((x) - (y)) < 1e-5f)

static bool compareFloat(auto a, auto b) {
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i)
        if (std::fabs(a[i] - b[i]) > 1e-5f)
            return false;

    return true;
}

SShader::SShader() {
    uniformLocations.fill(-1);
}

SShader::~SShader() {
    destroy();
}

void SShader::createVao() {
    GLuint shaderVao = 0, shaderVbo = 0, shaderVboUv = 0;

    glGenVertexArrays(1, &shaderVao);
    glBindVertexArray(shaderVao);

    if (uniformLocations[SHADER_POS_ATTRIB] != -1) {
        glGenBuffers(1, &shaderVbo);
        glBindBuffer(GL_ARRAY_BUFFER, shaderVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), fullVerts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(uniformLocations[SHADER_POS_ATTRIB]);
        glVertexAttribPointer(uniformLocations[SHADER_POS_ATTRIB], 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    // UV VBO (dynamic, may be updated per frame)
    if (uniformLocations[SHADER_TEX_ATTRIB] != -1) {
        glGenBuffers(1, &shaderVboUv);
        glBindBuffer(GL_ARRAY_BUFFER, shaderVboUv);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), fullVerts, GL_DYNAMIC_DRAW); // Initial dummy UVs
        glEnableVertexAttribArray(uniformLocations[SHADER_TEX_ATTRIB]);
        glVertexAttribPointer(uniformLocations[SHADER_TEX_ATTRIB], 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    uniformLocations[SHADER_SHADER_VAO]     = shaderVao;
    uniformLocations[SHADER_SHADER_VBO_POS] = shaderVbo;
    uniformLocations[SHADER_SHADER_VBO_UV]  = shaderVboUv;

    RASSERT(uniformLocations[SHADER_SHADER_VAO] >= 0, "SHADER_SHADER_VAO could not be created");
    RASSERT(uniformLocations[SHADER_SHADER_VBO_POS] >= 0, "SHADER_SHADER_VBO_POS could not be created");
    RASSERT(uniformLocations[SHADER_SHADER_VBO_UV] >= 0, "SHADER_SHADER_VBO_UV could not be created");
}

void SShader::setUniformInt(eShaderUniform location, GLint v0) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0 && std::get<GLint>(cached) == v0)
        return;

    cached = v0;
    glUniform1i(uniformLocations[location], v0);
}

void SShader::setUniformFloat(eShaderUniform location, GLfloat v0) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<GLfloat>(cached);
        if (EPSILON(val, v0))
            return;
    }

    cached = v0;
    glUniform1f(uniformLocations[location], v0);
}

void SShader::setUniformFloat2(eShaderUniform location, GLfloat v0, GLfloat v1) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<std::array<GLfloat, 2>>(cached);
        if (EPSILON(val[0], v0) && EPSILON(val[1], v1))
            return;
    }

    cached = std::array<GLfloat, 2>{v0, v1};
    glUniform2f(uniformLocations[location], v0, v1);
}

void SShader::setUniformFloat3(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<std::array<GLfloat, 3>>(cached);
        if (EPSILON(val[0], v0) && EPSILON(val[1], v1) && EPSILON(val[2], v2))
            return;
    }

    cached = std::array<GLfloat, 3>{v0, v1, v2};
    glUniform3f(uniformLocations[location], v0, v1, v2);
}

void SShader::setUniformFloat4(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<std::array<GLfloat, 4>>(cached);
        if (EPSILON(val[0], v0) && EPSILON(val[1], v1) && EPSILON(val[2], v2) && EPSILON(val[3], v3))
            return;
    }

    cached = std::array<GLfloat, 4>{v0, v1, v2, v3};
    glUniform4f(uniformLocations[location], v0, v1, v2, v3);
}

void SShader::setUniformMatrix3fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 9> value) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<SUniformMatrix3Data>(cached);
        if (val.count == count && val.transpose == transpose && compareFloat(val.value, value))
            return;
    }

    cached = SUniformMatrix3Data{.count = count, .transpose = transpose, .value = value};
    glUniformMatrix3fv(uniformLocations[location], count, transpose, value.data());
}

void SShader::setUniformMatrix4x2fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 8> value) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<SUniformMatrix4Data>(cached);
        if (val.count == count && val.transpose == transpose && compareFloat(val.value, value))
            return;
    }

    cached = SUniformMatrix4Data{.count = count, .transpose = transpose, .value = value};
    glUniformMatrix4x2fv(uniformLocations[location], count, transpose, value.data());
}

void SShader::setUniform4fv(eShaderUniform location, GLsizei count, std::vector<float> value) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<SUniform4Data>(cached);
        if (val.count == count && compareFloat(val.value, value))
            return;
    }

    cached = SUniform4Data{.count = count, .value = value};
    glUniform4fv(uniformLocations[location], count, value.data());
}

void SShader::destroy() {
    uniformStatus.fill(std::monostate());

    if (program == 0)
        return;

    GLuint shaderVao, shaderVbo, shaderVboUv;

    shaderVao   = uniformLocations[SHADER_SHADER_VAO] == -1 ? 0 : uniformLocations[SHADER_SHADER_VAO];
    shaderVbo   = uniformLocations[SHADER_SHADER_VBO_POS] == -1 ? 0 : uniformLocations[SHADER_SHADER_VBO_POS];
    shaderVboUv = uniformLocations[SHADER_SHADER_VBO_UV] == -1 ? 0 : uniformLocations[SHADER_SHADER_VBO_UV];

    if (shaderVao)
        glDeleteVertexArrays(1, &shaderVao);

    if (shaderVbo)
        glDeleteBuffers(1, &shaderVbo);

    if (shaderVboUv)
        glDeleteBuffers(1, &shaderVboUv);

    glDeleteProgram(program);
    program = 0;
}
