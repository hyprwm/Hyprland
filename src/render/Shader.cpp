#include "Shader.hpp"
#include "render/OpenGL.hpp"

SShader::~SShader() {
    destroy();
}

void SShader::createVao() {
    glGenVertexArrays(1, &shaderVao);
    glBindVertexArray(shaderVao);

    if (posAttrib != -1) {
        glGenBuffers(1, &shaderVboPos);
        glBindBuffer(GL_ARRAY_BUFFER, shaderVboPos);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), fullVerts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    // UV VBO (dynamic, may be updated per frame)
    if (texAttrib != -1) {
        glGenBuffers(1, &shaderVboUv);
        glBindBuffer(GL_ARRAY_BUFFER, shaderVboUv);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), fullVerts, GL_DYNAMIC_DRAW); // Initial dummy UVs
        glEnableVertexAttribArray(texAttrib);
        glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SShader::setUniformInt(GLint location, GLint v0) {
    auto it = uniformStatusInt.find(location);

    if (it != uniformStatusInt.end() && it->second == v0)
        return;

    uniformStatusInt[location] = v0;
    glUniform1i(location, v0);
}

void SShader::setUniformFloat(GLint location, GLfloat v0) {
    auto it = uniformStatusFloat.find(location);

    if (it != uniformStatusFloat.end() && std::fabs(it->second - v0) < 1e-5f)
        return;

    uniformStatusInt[location] = v0;
    glUniform1f(location, v0);
}

void SShader::setUniformFloat2(GLint location, GLfloat v0, GLfloat v1) {
    auto it = uniformStatusFloat2.find(location);

    if (it != uniformStatusFloat2.end() && std::fabs(it->second[0] - v0) < 1e-5f && std::fabs(it->second[1] - v1) < 1e-5f)
        return;

    uniformStatusFloat2[location] = {v0, v1};
    glUniform2f(location, v0, v1);
}

void SShader::setUniformFloat3(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    auto it = uniformStatusFloat3.find(location);

    if (it != uniformStatusFloat3.end() && std::fabs(it->second[0] - v0) < 1e-5f && std::fabs(it->second[1] - v1) < 1e-5f && std::fabs(it->second[2] - v2) < 1e-5f)
        return;

    uniformStatusFloat3[location] = {v0, v1, v2};
    glUniform3f(location, v0, v1, v2);
}

void SShader::setUniformFloat4(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    auto it = uniformStatusFloat4.find(location);

    if (it != uniformStatusFloat4.end() && std::fabs(it->second[0] - v0) < 1e-5f && std::fabs(it->second[1] - v1) < 1e-5f && std::fabs(it->second[2] - v2) < 1e-5f &&
        std::fabs(it->second[3] - v3) < 1e-5f)
        return;

    uniformStatusFloat4[location] = {v0, v1, v2, v3};
    glUniform4f(location, v0, v1, v2, v3);
}

void SShader::setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, std::array<GLfloat, 9> value) {
    auto it = uniformStatusMatrix3fv.find(location);

    // because floating point comparisions sucks
    auto compareFloat = [](auto a, auto b) {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (std::fabs(a[i] - b[i]) > 1e-5f)
                return false;

        return true;
    };

    if (it != uniformStatusMatrix3fv.end() && it->second.count == count && it->second.transpose == transpose && compareFloat(it->second.value, value))
        return;

    uniformStatusMatrix3fv[location] = {.count = count, .transpose = transpose, .value = value};
    glUniformMatrix3fv(location, count, transpose, value.data());
}

void SShader::setUniformMatrix42xfv(GLint location, GLsizei count, GLboolean transpose, std::array<GLfloat, 8> value) {
    auto it = uniformStatusMatrix4x2fv.find(location);

    // because floating point comparisions sucks
    auto compareFloat = [](auto a, auto b) {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (std::fabs(a[i] - b[i]) > 1e-5f)
                return false;

        return true;
    };

    if (it != uniformStatusMatrix4x2fv.end() && it->second.count == count && it->second.transpose == transpose && compareFloat(it->second.value, value))
        return;

    uniformStatusMatrix4x2fv[location] = {.count = count, .transpose = transpose, .value = value};
    glUniformMatrix3fv(location, count, transpose, value.data());
}

void SShader::setUniform4fv(GLint location, GLsizei count, std::vector<float> value) {
    auto it = uniformStatus4fv.find(location);

    // because floating point comparisions sucks
    auto compareFloat = [](auto a, auto b) {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (std::fabs(a[i] - b[i]) > 1e-5f)
                return false;

        return true;
    };

    if (it != uniformStatus4fv.end() && compareFloat(it->second.value, value))
        return;

    uniformStatus4fv[location] = {.count = count, .value = value};
    glUniform4fv(location, count, value.data());
}

void SShader::destroy() {
    uniformStatusInt.clear();
    uniformStatusFloat.clear();
    uniformStatusFloat2.clear();
    uniformStatusFloat3.clear();
    uniformStatusFloat4.clear();
    uniformStatusMatrix3fv.clear();
    uniformStatusMatrix4x2fv.clear();
    uniformStatus4fv.clear();

    if (program == 0)
        return;

    if (shaderVao)
        glDeleteVertexArrays(1, &shaderVao);

    if (shaderVboPos)
        glDeleteBuffers(1, &shaderVboPos);

    if (shaderVboUv)
        glDeleteBuffers(1, &shaderVboUv);

    glDeleteProgram(program);
    program = 0;
}
