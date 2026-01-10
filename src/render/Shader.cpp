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
    m_uniformLocations.fill(-1);
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

    m_uniformLocations[SHADER_PROJ]        = getUniform("proj");
    m_uniformLocations[SHADER_COLOR]       = getUniform("color");
    m_uniformLocations[SHADER_ALPHA_MATTE] = getUniform("texMatte");
    m_uniformLocations[SHADER_TEX_TYPE]    = getUniform("texType");

    // shader has #include "CM.glsl"
    m_uniformLocations[SHADER_SKIP_CM]           = getUniform("skipCM");
    m_uniformLocations[SHADER_SOURCE_TF]         = getUniform("sourceTF");
    m_uniformLocations[SHADER_TARGET_TF]         = getUniform("targetTF");
    m_uniformLocations[SHADER_SRC_TF_RANGE]      = getUniform("srcTFRange");
    m_uniformLocations[SHADER_DST_TF_RANGE]      = getUniform("dstTFRange");
    m_uniformLocations[SHADER_TARGET_PRIMARIES]  = getUniform("targetPrimaries");
    m_uniformLocations[SHADER_MAX_LUMINANCE]     = getUniform("maxLuminance");
    m_uniformLocations[SHADER_SRC_REF_LUMINANCE] = getUniform("srcRefLuminance");
    m_uniformLocations[SHADER_DST_MAX_LUMINANCE] = getUniform("dstMaxLuminance");
    m_uniformLocations[SHADER_DST_REF_LUMINANCE] = getUniform("dstRefLuminance");
    m_uniformLocations[SHADER_SDR_SATURATION]    = getUniform("sdrSaturation");
    m_uniformLocations[SHADER_SDR_BRIGHTNESS]    = getUniform("sdrBrightnessMultiplier");
    m_uniformLocations[SHADER_CONVERT_MATRIX]    = getUniform("convertMatrix");
    //
    m_uniformLocations[SHADER_TEX]                 = getUniform("tex");
    m_uniformLocations[SHADER_ALPHA]               = getUniform("alpha");
    m_uniformLocations[SHADER_POS_ATTRIB]          = getAttrib("pos");
    m_uniformLocations[SHADER_TEX_ATTRIB]          = getAttrib("texcoord");
    m_uniformLocations[SHADER_MATTE_TEX_ATTRIB]    = getAttrib("texcoordMatte");
    m_uniformLocations[SHADER_DISCARD_OPAQUE]      = getUniform("discardOpaque");
    m_uniformLocations[SHADER_DISCARD_ALPHA]       = getUniform("discardAlpha");
    m_uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = getUniform("discardAlphaValue");
    /* set in createVao
        m_uniformLocations[SHADER_SHADER_VAO]
        m_uniformLocations[SHADER_SHADER_VBO_POS]
        m_uniformLocations[SHADER_SHADER_VBO_UV]
        */
    m_uniformLocations[SHADER_TOP_LEFT]     = getUniform("topLeft");
    m_uniformLocations[SHADER_BOTTOM_RIGHT] = getUniform("bottomRight");

    // compat for screenshaders
    auto fullSize = getUniform("fullSize");
    if (fullSize == -1)
        fullSize = getUniform("screen_size");
    if (fullSize == -1)
        fullSize = getUniform("screenSize");
    m_uniformLocations[SHADER_FULL_SIZE] = fullSize;

    m_uniformLocations[SHADER_FULL_SIZE_UNTRANSFORMED]   = getUniform("fullSizeUntransformed");
    m_uniformLocations[SHADER_RADIUS]                    = getUniform("radius");
    m_uniformLocations[SHADER_RADIUS_OUTER]              = getUniform("radiusOuter");
    m_uniformLocations[SHADER_ROUNDING_POWER]            = getUniform("roundingPower");
    m_uniformLocations[SHADER_THICK]                     = getUniform("thick");
    m_uniformLocations[SHADER_HALFPIXEL]                 = getUniform("halfpixel");
    m_uniformLocations[SHADER_RANGE]                     = getUniform("range");
    m_uniformLocations[SHADER_SHADOW_POWER]              = getUniform("shadowPower");
    m_uniformLocations[SHADER_USE_ALPHA_MATTE]           = getUniform("useAlphaMatte");
    m_uniformLocations[SHADER_APPLY_TINT]                = getUniform("applyTint");
    m_uniformLocations[SHADER_TINT]                      = getUniform("tint");
    m_uniformLocations[SHADER_GRADIENT]                  = getUniform("gradient");
    m_uniformLocations[SHADER_GRADIENT_LENGTH]           = getUniform("gradientLength");
    m_uniformLocations[SHADER_GRADIENT2]                 = getUniform("gradient2");
    m_uniformLocations[SHADER_GRADIENT2_LENGTH]          = getUniform("gradient2Length");
    m_uniformLocations[SHADER_ANGLE]                     = getUniform("angle");
    m_uniformLocations[SHADER_ANGLE2]                    = getUniform("angle2");
    m_uniformLocations[SHADER_GRADIENT_LERP]             = getUniform("gradientLerp");
    m_uniformLocations[SHADER_TIME]                      = getUniform("time");
    m_uniformLocations[SHADER_DISTORT]                   = getUniform("distort");
    m_uniformLocations[SHADER_WL_OUTPUT]                 = getUniform("wl_output");
    m_uniformLocations[SHADER_CONTRAST]                  = getUniform("contrast");
    m_uniformLocations[SHADER_PASSES]                    = getUniform("passes");
    m_uniformLocations[SHADER_VIBRANCY]                  = getUniform("vibrancy");
    m_uniformLocations[SHADER_VIBRANCY_DARKNESS]         = getUniform("vibrancy_darkness");
    m_uniformLocations[SHADER_BRIGHTNESS]                = getUniform("brightness");
    m_uniformLocations[SHADER_NOISE]                     = getUniform("noise");
    m_uniformLocations[SHADER_POINTER]                   = getUniform("pointer_position");
    m_uniformLocations[SHADER_POINTER_SHAPE]             = getUniform("pointer_shape");
    m_uniformLocations[SHADER_POINTER_SWITCH_TIME]       = getUniform("pointer_switch_time");
    m_uniformLocations[SHADER_POINTER_SHAPE_PREVIOUS]    = getUniform("pointer_shape_previous");
    m_uniformLocations[SHADER_POINTER_PRESSED_POSITIONS] = getUniform("pointer_pressed_positions");
    m_uniformLocations[SHADER_POINTER_HIDDEN]            = getUniform("pointer_hidden");
    m_uniformLocations[SHADER_POINTER_KILLING]           = getUniform("pointer_killing");
    m_uniformLocations[SHADER_POINTER_PRESSED_TIMES]     = getUniform("pointer_pressed_times");
    m_uniformLocations[SHADER_POINTER_PRESSED_KILLED]    = getUniform("pointer_pressed_killed");
    m_uniformLocations[SHADER_POINTER_PRESSED_TOUCHED]   = getUniform("pointer_pressed_touched");
    m_uniformLocations[SHADER_POINTER_INACTIVE_TIMEOUT]  = getUniform("pointer_inactive_timeout");
    m_uniformLocations[SHADER_POINTER_LAST_ACTIVE]       = getUniform("pointer_last_active");
    m_uniformLocations[SHADER_POINTER_SIZE]              = getUniform("pointer_size");
}

void CShader::createVao() {
    GLuint shaderVao = 0, shaderVbo = 0, shaderVboUv = 0;

    glGenVertexArrays(1, &shaderVao);
    glBindVertexArray(shaderVao);

    if (m_uniformLocations[SHADER_POS_ATTRIB] != -1) {
        glGenBuffers(1, &shaderVbo);
        glBindBuffer(GL_ARRAY_BUFFER, shaderVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), fullVerts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(m_uniformLocations[SHADER_POS_ATTRIB]);
        glVertexAttribPointer(m_uniformLocations[SHADER_POS_ATTRIB], 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    // UV VBO (dynamic, may be updated per frame)
    if (m_uniformLocations[SHADER_TEX_ATTRIB] != -1) {
        glGenBuffers(1, &shaderVboUv);
        glBindBuffer(GL_ARRAY_BUFFER, shaderVboUv);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), fullVerts, GL_DYNAMIC_DRAW); // Initial dummy UVs
        glEnableVertexAttribArray(m_uniformLocations[SHADER_TEX_ATTRIB]);
        glVertexAttribPointer(m_uniformLocations[SHADER_TEX_ATTRIB], 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_uniformLocations[SHADER_SHADER_VAO]     = shaderVao;
    m_uniformLocations[SHADER_SHADER_VBO_POS] = shaderVbo;
    m_uniformLocations[SHADER_SHADER_VBO_UV]  = shaderVboUv;

    RASSERT(m_uniformLocations[SHADER_SHADER_VAO] >= 0, "SHADER_SHADER_VAO could not be created");
    RASSERT(m_uniformLocations[SHADER_SHADER_VBO_POS] >= 0, "SHADER_SHADER_VBO_POS could not be created");
    RASSERT(m_uniformLocations[SHADER_SHADER_VBO_UV] >= 0, "SHADER_SHADER_VBO_UV could not be created");
}

void CShader::setUniformInt(eShaderUniform location, GLint v0) {
    if (m_uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0 && std::get<GLint>(cached) == v0)
        return;

    cached = v0;
    glUniform1i(m_uniformLocations[location], v0);
}

void CShader::setUniformFloat(eShaderUniform location, GLfloat v0) {
    if (m_uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<GLfloat>(cached);
        if (EPSILON(val, v0))
            return;
    }

    cached = v0;
    glUniform1f(m_uniformLocations[location], v0);
}

void CShader::setUniformFloat2(eShaderUniform location, GLfloat v0, GLfloat v1) {
    if (m_uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<std::array<GLfloat, 2>>(cached);
        if (EPSILON(val[0], v0) && EPSILON(val[1], v1))
            return;
    }

    cached = std::array<GLfloat, 2>{v0, v1};
    glUniform2f(m_uniformLocations[location], v0, v1);
}

void CShader::setUniformFloat3(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2) {
    if (m_uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<std::array<GLfloat, 3>>(cached);
        if (EPSILON(val[0], v0) && EPSILON(val[1], v1) && EPSILON(val[2], v2))
            return;
    }

    cached = std::array<GLfloat, 3>{v0, v1, v2};
    glUniform3f(m_uniformLocations[location], v0, v1, v2);
}

void CShader::setUniformFloat4(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    if (m_uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<std::array<GLfloat, 4>>(cached);
        if (EPSILON(val[0], v0) && EPSILON(val[1], v1) && EPSILON(val[2], v2) && EPSILON(val[3], v3))
            return;
    }

    cached = std::array<GLfloat, 4>{v0, v1, v2, v3};
    glUniform4f(m_uniformLocations[location], v0, v1, v2, v3);
}

void CShader::setUniformMatrix3fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 9> value) {
    if (m_uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<SUniformMatrix3Data>(cached);
        if (val.count == count && val.transpose == transpose && compareFloat(val.value, value))
            return;
    }

    cached = SUniformMatrix3Data{.count = count, .transpose = transpose, .value = value};
    glUniformMatrix3fv(m_uniformLocations[location], count, transpose, value.data());
}

void CShader::setUniformMatrix4x2fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 8> value) {
    if (m_uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<SUniformMatrix4Data>(cached);
        if (val.count == count && val.transpose == transpose && compareFloat(val.value, value))
            return;
    }

    cached = SUniformMatrix4Data{.count = count, .transpose = transpose, .value = value};
    glUniformMatrix4x2fv(m_uniformLocations[location], count, transpose, value.data());
}

void CShader::setUniformfv(eShaderUniform location, GLsizei count, const std::vector<float>& value, GLsizei vec_size) {
    if (m_uniformLocations.at(location) == -1)
        return;

    auto& cached = uniformStatus.at(location);

    if (cached.index() != 0) {
        auto val = std::get<SUniformVData>(cached);
        if (val.count == count && compareFloat(val.value, value))
            return;
    }

    cached = SUniformVData{.count = count, .value = value};
    switch (vec_size) {
        case 1: glUniform1fv(m_uniformLocations[location], count, value.data()); break;
        case 2: glUniform2fv(m_uniformLocations[location], count, value.data()); break;
        case 4: glUniform4fv(m_uniformLocations[location], count, value.data()); break;
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

    shaderVao   = m_uniformLocations[SHADER_SHADER_VAO] == -1 ? 0 : m_uniformLocations[SHADER_SHADER_VAO];
    shaderVbo   = m_uniformLocations[SHADER_SHADER_VBO_POS] == -1 ? 0 : m_uniformLocations[SHADER_SHADER_VBO_POS];
    shaderVboUv = m_uniformLocations[SHADER_SHADER_VBO_UV] == -1 ? 0 : m_uniformLocations[SHADER_SHADER_VBO_UV];

    if (shaderVao)
        glDeleteVertexArrays(1, &shaderVao);

    if (shaderVbo)
        glDeleteBuffers(1, &shaderVbo);

    if (shaderVboUv)
        glDeleteBuffers(1, &shaderVboUv);

    glDeleteProgram(m_program);
    m_program = 0;
}

GLint CShader::getUniformLocation(eShaderUniform location) const {
    return m_uniformLocations[location];
}

GLuint CShader::program() const {
    return m_program;
}

int CShader::getInitialTime() const {
    return m_initialTime;
}

void CShader::setInitialTime(int time) {
    m_initialTime = time;
}
