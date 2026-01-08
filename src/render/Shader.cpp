#include "Shader.hpp"
#include "../config/ConfigManager.hpp"
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

CShader::CShader() {
    uniformLocations.fill(-1);
}

CShader::~CShader() {
    destroy();
}

void CShader::logShaderError(const GLuint& shader, bool program, bool silent) {
    GLint maxLength = 0;
    if (program)
        glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    else
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    std::vector<GLchar> errorLog(maxLength);
    if (program)
        glGetProgramInfoLog(shader, maxLength, &maxLength, errorLog.data());
    else
        glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog.data());
    std::string errorStr(errorLog.begin(), errorLog.end());

    const auto  FULLERROR = (program ? "Screen shader parser: Error linking program:" : "Screen shader parser: Error compiling shader: ") + errorStr;

    Log::logger->log(Log::ERR, "Failed to link shader: {}", FULLERROR);

    if (!silent)
        g_pConfigManager->addParseError(FULLERROR);
}

GLuint CShader::compileShader(const GLuint& type, std::string src, bool dynamic, bool silent) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (dynamic) {
        if (ok == GL_FALSE) {
            logShaderError(shader, false, silent);
            return 0;
        }
    } else {
        if (ok != GL_TRUE)
            logShaderError(shader, false);
        RASSERT(ok != GL_FALSE, "compileShader() failed! GL_COMPILE_STATUS not OK!");
    }

    return shader;
}

bool CShader::createProgram(const std::string& vert, const std::string& frag, bool dynamic, bool silent) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert, dynamic, silent);
    if (dynamic) {
        if (vertCompiled == 0)
            return false;
    } else
        RASSERT(vertCompiled, "Compiling shader failed. VERTEX nullptr! Shader source:\n\n{}", vert);

    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag, dynamic, silent);
    if (dynamic) {
        if (fragCompiled == 0)
            return false;
    } else
        RASSERT(fragCompiled, "Compiling shader failed. FRAGMENT nullptr! Shader source:\n\n{}", frag);

    auto prog = glCreateProgram();
    glAttachShader(prog, vertCompiled);
    glAttachShader(prog, fragCompiled);
    glLinkProgram(prog);

    glDetachShader(prog, vertCompiled);
    glDetachShader(prog, fragCompiled);
    glDeleteShader(vertCompiled);
    glDeleteShader(fragCompiled);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (dynamic) {
        if (ok == GL_FALSE) {
            logShaderError(prog, true, silent);
            return false;
        }
    } else {
        if (ok != GL_TRUE)
            logShaderError(prog, true);
        RASSERT(ok != GL_FALSE, "createProgram() failed! GL_LINK_STATUS not OK!");
    }

    m_program = prog;
    return true;
}

void CShader::createVao() {
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

void CShader::setUniformInt(eShaderUniform location, GLint v0) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0 && std::get<GLint>(cached) == v0)
        return;

    cached = v0;
    glUniform1i(uniformLocations[location], v0);
}

void CShader::setUniformFloat(eShaderUniform location, GLfloat v0) {
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

void CShader::setUniformFloat2(eShaderUniform location, GLfloat v0, GLfloat v1) {
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

void CShader::setUniformFloat3(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2) {
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

void CShader::setUniformFloat4(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
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

void CShader::setUniformMatrix3fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 9> value) {
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

void CShader::setUniformMatrix4x2fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 8> value) {
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

void CShader::setUniformfv(eShaderUniform location, GLsizei count, const std::vector<float>& value, GLsizei vec_size) {
    if (uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<SUniformVData>(cached);
        if (val.count == count && compareFloat(val.value, value))
            return;
    }

    cached = SUniformVData{.count = count, .value = value};
    switch (vec_size) {
        case 1: glUniform1fv(uniformLocations[location], count, value.data()); break;
        case 2: glUniform2fv(uniformLocations[location], count, value.data()); break;
        case 4: glUniform4fv(uniformLocations[location], count, value.data()); break;
        default: UNREACHABLE();
    }
}

void CShader::setUniform1fv(eShaderUniform location, GLsizei count, const std::vector<float>& value) {
    setUniformfv(location, count, value, 1);
}

void CShader::setUniform2fv(eShaderUniform location, GLsizei count, const std::vector<float>& value) {
    setUniformfv(location, count, value, 2);
}

void CShader::setUniform4fv(eShaderUniform location, GLsizei count, const std::vector<float>& value) {
    setUniformfv(location, count, value, 4);
}

void CShader::destroy() {
    uniformStatus.fill(std::monostate());

    if (m_program == 0)
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

    glDeleteProgram(m_program);
    m_program = 0;
}

GLuint CShader::program() const {
    return m_program;
}
