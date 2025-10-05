#pragma once

#include "../defines.hpp"
#include <array>
#include <variant>

enum eShaderUniform : uint8_t {
    SHADER_PROJ = 0,
    SHADER_COLOR,
    SHADER_ALPHA_MATTE,
    SHADER_TEX_TYPE,
    SHADER_SKIP_CM,
    SHADER_SOURCE_TF,
    SHADER_TARGET_TF,
    SHADER_SRC_TF_RANGE,
    SHADER_DST_TF_RANGE,
    SHADER_TARGET_PRIMARIES,
    SHADER_MAX_LUMINANCE,
    SHADER_DST_MAX_LUMINANCE,
    SHADER_DST_REF_LUMINANCE,
    SHADER_SDR_SATURATION,
    SHADER_SDR_BRIGHTNESS,
    SHADER_CONVERT_MATRIX,
    SHADER_TEX,
    SHADER_ALPHA,
    SHADER_POS_ATTRIB,
    SHADER_TEX_ATTRIB,
    SHADER_MATTE_TEX_ATTRIB,
    SHADER_DISCARD_OPAQUE,
    SHADER_DISCARD_ALPHA,
    SHADER_DISCARD_ALPHA_VALUE,
    SHADER_SHADER_VAO,
    SHADER_SHADER_VBO_POS,
    SHADER_SHADER_VBO_UV,
    SHADER_TOP_LEFT,
    SHADER_BOTTOM_RIGHT,
    SHADER_FULL_SIZE,
    SHADER_FULL_SIZE_UNTRANSFORMED,
    SHADER_RADIUS,
    SHADER_RADIUS_OUTER,
    SHADER_ROUNDING_POWER,
    SHADER_THICK,
    SHADER_HALFPIXEL,
    SHADER_RANGE,
    SHADER_SHADOW_POWER,
    SHADER_USE_ALPHA_MATTE,
    SHADER_APPLY_TINT,
    SHADER_TINT,
    SHADER_GRADIENT,
    SHADER_GRADIENT_LENGTH,
    SHADER_ANGLE,
    SHADER_GRADIENT2,
    SHADER_GRADIENT2_LENGTH,
    SHADER_ANGLE2,
    SHADER_GRADIENT_LERP,
    SHADER_TIME,
    SHADER_DISTORT,
    SHADER_WL_OUTPUT,
    SHADER_CONTRAST,
    SHADER_PASSES,
    SHADER_VIBRANCY,
    SHADER_VIBRANCY_DARKNESS,
    SHADER_BRIGHTNESS,
    SHADER_NOISE,
    SHADER_POINTER,
    SHADER_POINTER_SHAPE,
    SHADER_POINTER_SWITCH_TIME,
    SHADER_POINTER_SHAPE_PREVIOUS,
    SHADER_POINTER_PRESSED_POSITIONS,
    SHADER_POINTER_HIDDEN,
    SHADER_POINTER_KILLING,
    SHADER_POINTER_PRESSED_TIMES,
    SHADER_POINTER_PRESSED_KILLED,
    SHADER_POINTER_PRESSED_TOUCHED,
    SHADER_POINTER_INACTIVE_TIMEOUT,
    SHADER_POINTER_LAST_ACTIVE,
    SHADER_POINTER_SIZE,
    SHADER_CAPTURE,

    SHADER_LAST,
};

struct SShader {
    SShader();
    ~SShader();

    GLuint                         program = 0;

    std::array<GLint, SHADER_LAST> uniformLocations;

    float                          initialTime = 0;

    struct SUniformMatrix3Data {
        GLsizei                count     = 0;
        GLboolean              transpose = false;
        std::array<GLfloat, 9> value     = {};
    };

    struct SUniformMatrix4Data {
        GLsizei                count     = 0;
        GLboolean              transpose = false;
        std::array<GLfloat, 8> value     = {};
    };

    struct SUniformVData {
        GLsizei            count = 0;
        std::vector<float> value;
    };

    //
    std::array<std::variant<std::monostate, GLint, GLfloat, std::array<GLfloat, 2>, std::array<GLfloat, 3>, std::array<GLfloat, 4>, SUniformMatrix3Data, SUniformMatrix4Data,
                            SUniformVData>,
               SHADER_LAST>
        uniformStatus;
    //

    void createVao();
    void setUniformInt(eShaderUniform location, GLint v0);
    void setUniformFloat(eShaderUniform location, GLfloat v0);
    void setUniformFloat2(eShaderUniform location, GLfloat v0, GLfloat v1);
    void setUniformFloat3(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2);
    void setUniformFloat4(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void setUniformMatrix3fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 9> value);
    void setUniformMatrix4x2fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 8> value);
    void setUniform1fv(eShaderUniform location, GLsizei count, const std::vector<float>& value);
    void setUniform2fv(eShaderUniform location, GLsizei count, const std::vector<float>& value);
    void setUniform4fv(eShaderUniform location, GLsizei count, const std::vector<float>& value);
    void destroy();

  private:
    void setUniformfv(eShaderUniform location, GLsizei count, const std::vector<float>& value, GLsizei vec_size);
};
