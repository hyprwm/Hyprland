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

    getUniformLocations();
    createVao();
    return true;
}

// its fine to call glGet on shaders that dont have the uniform
// this however hardcodes the name now. #TODO maybe dont
void CShader::getUniformLocations() {
    auto getUniform = [this](const GLchar* name) { return glGetUniformLocation(m_program, name); };
    auto getAttrib  = [this](const GLchar* name) { return glGetAttribLocation(m_program, name); };

    uniformLocations[SHADER_PROJ]        = getUniform("proj");
    uniformLocations[SHADER_COLOR]       = getUniform("color");
    uniformLocations[SHADER_ALPHA_MATTE] = getUniform("texMatte");
    uniformLocations[SHADER_TEX_TYPE]    = getUniform("texType");

    // shader has #include "CM.glsl"
    uniformLocations[SHADER_SKIP_CM]           = getUniform("skipCM");
    uniformLocations[SHADER_SOURCE_TF]         = getUniform("sourceTF");
    uniformLocations[SHADER_TARGET_TF]         = getUniform("targetTF");
    uniformLocations[SHADER_SRC_TF_RANGE]      = getUniform("srcTFRange");
    uniformLocations[SHADER_DST_TF_RANGE]      = getUniform("dstTFRange");
    uniformLocations[SHADER_TARGET_PRIMARIES]  = getUniform("targetPrimaries");
    uniformLocations[SHADER_MAX_LUMINANCE]     = getUniform("maxLuminance");
    uniformLocations[SHADER_SRC_REF_LUMINANCE] = getUniform("srcRefLuminance");
    uniformLocations[SHADER_DST_MAX_LUMINANCE] = getUniform("dstMaxLuminance");
    uniformLocations[SHADER_DST_REF_LUMINANCE] = getUniform("dstRefLuminance");
    uniformLocations[SHADER_SDR_SATURATION]    = getUniform("sdrSaturation");
    uniformLocations[SHADER_SDR_BRIGHTNESS]    = getUniform("sdrBrightnessMultiplier");
    uniformLocations[SHADER_CONVERT_MATRIX]    = getUniform("convertMatrix");
    //
    uniformLocations[SHADER_TEX]                 = getUniform("tex");
    uniformLocations[SHADER_ALPHA]               = getUniform("alpha");
    uniformLocations[SHADER_POS_ATTRIB]          = getAttrib("pos");
    uniformLocations[SHADER_TEX_ATTRIB]          = getAttrib("texcoord");
    uniformLocations[SHADER_MATTE_TEX_ATTRIB]    = getAttrib("texcoordMatte");
    uniformLocations[SHADER_DISCARD_OPAQUE]      = getUniform("discardOpaque");
    uniformLocations[SHADER_DISCARD_ALPHA]       = getUniform("discardAlpha");
    uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = getUniform("discardAlphaValue");
    /* set in createVao
        uniformLocations[SHADER_SHADER_VAO]
        uniformLocations[SHADER_SHADER_VBO_POS]
        uniformLocations[SHADER_SHADER_VBO_UV]
        */
    uniformLocations[SHADER_TOP_LEFT]     = getUniform("topLeft");
    uniformLocations[SHADER_BOTTOM_RIGHT] = getUniform("bottomRight");

    // compat for screenshaders
    auto fullSize = getUniform("fullSize");
    if (fullSize == -1)
        fullSize = getUniform("screen_size");
    if (fullSize == -1)
        fullSize = getUniform("screenSize");
    uniformLocations[SHADER_FULL_SIZE] = fullSize;

    uniformLocations[SHADER_FULL_SIZE_UNTRANSFORMED]   = getUniform("fullSizeUntransformed");
    uniformLocations[SHADER_RADIUS]                    = getUniform("radius");
    uniformLocations[SHADER_RADIUS_OUTER]              = getUniform("radiusOuter");
    uniformLocations[SHADER_ROUNDING_POWER]            = getUniform("roundingPower");
    uniformLocations[SHADER_THICK]                     = getUniform("thick");
    uniformLocations[SHADER_HALFPIXEL]                 = getUniform("halfpixel");
    uniformLocations[SHADER_RANGE]                     = getUniform("range");
    uniformLocations[SHADER_SHADOW_POWER]              = getUniform("shadowPower");
    uniformLocations[SHADER_USE_ALPHA_MATTE]           = getUniform("useAlphaMatte");
    uniformLocations[SHADER_APPLY_TINT]                = getUniform("applyTint");
    uniformLocations[SHADER_TINT]                      = getUniform("tint");
    uniformLocations[SHADER_GRADIENT]                  = getUniform("gradient");
    uniformLocations[SHADER_GRADIENT_LENGTH]           = getUniform("gradientLength");
    uniformLocations[SHADER_GRADIENT2]                 = getUniform("gradient2");
    uniformLocations[SHADER_GRADIENT2_LENGTH]          = getUniform("gradient2Length");
    uniformLocations[SHADER_ANGLE]                     = getUniform("angle");
    uniformLocations[SHADER_ANGLE2]                    = getUniform("angle2");
    uniformLocations[SHADER_GRADIENT_LERP]             = getUniform("gradientLerp");
    uniformLocations[SHADER_TIME]                      = getUniform("time");
    uniformLocations[SHADER_DISTORT]                   = getUniform("distort");
    uniformLocations[SHADER_WL_OUTPUT]                 = getUniform("wl_output");
    uniformLocations[SHADER_CONTRAST]                  = getUniform("contrast");
    uniformLocations[SHADER_PASSES]                    = getUniform("passes");
    uniformLocations[SHADER_VIBRANCY]                  = getUniform("vibrancy");
    uniformLocations[SHADER_VIBRANCY_DARKNESS]         = getUniform("vibrancy_darkness");
    uniformLocations[SHADER_BRIGHTNESS]                = getUniform("brightness");
    uniformLocations[SHADER_NOISE]                     = getUniform("noise");
    uniformLocations[SHADER_POINTER]                   = getUniform("pointer_position");
    uniformLocations[SHADER_POINTER_SHAPE]             = getUniform("pointer_shape");
    uniformLocations[SHADER_POINTER_SWITCH_TIME]       = getUniform("pointer_switch_time");
    uniformLocations[SHADER_POINTER_SHAPE_PREVIOUS]    = getUniform("pointer_shape_previous");
    uniformLocations[SHADER_POINTER_PRESSED_POSITIONS] = getUniform("pointer_pressed_positions");
    uniformLocations[SHADER_POINTER_HIDDEN]            = getUniform("pointer_hidden");
    uniformLocations[SHADER_POINTER_KILLING]           = getUniform("pointer_killing");
    uniformLocations[SHADER_POINTER_PRESSED_TIMES]     = getUniform("pointer_pressed_times");
    uniformLocations[SHADER_POINTER_PRESSED_KILLED]    = getUniform("pointer_pressed_killed");
    uniformLocations[SHADER_POINTER_PRESSED_TOUCHED]   = getUniform("pointer_pressed_touched");
    uniformLocations[SHADER_POINTER_INACTIVE_TIMEOUT]  = getUniform("pointer_inactive_timeout");
    uniformLocations[SHADER_POINTER_LAST_ACTIVE]       = getUniform("pointer_last_active");
    uniformLocations[SHADER_POINTER_SIZE]              = getUniform("pointer_size");
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
