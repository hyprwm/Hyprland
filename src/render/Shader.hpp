#pragma once

#include "../defines.hpp"
#include <array>
#include <variant>

enum eShaderUniform : uint8_t {
    SHADER_PROJ = 0,
    SHADER_COLOR,
    SHADER_COLOR_SRGB,
    SHADER_ALPHA_MATTE,
    SHADER_TEX_TYPE,
    SHADER_SRC_TF_RANGE,
    SHADER_DST_TF_RANGE,
    SHADER_TARGET_PRIMARIES_XYZ,
    SHADER_MAX_LUMINANCE,
    SHADER_SRC_REF_LUMINANCE,
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
    SHADER_SHADER_VBO,
    SHADER_SHADER_UV_VAO,
    SHADER_SHADER_UV_VBO,
    SHADER_TOP_LEFT,
    SHADER_BOTTOM_RIGHT,
    SHADER_WINDOW_TOP_LEFT,
    SHADER_WINDOW_BOTTOM_RIGHT,
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
    SHADER_LUT_3D,
    SHADER_LUT_SIZE,
    SHADER_BLURRED_BG,
    SHADER_UV_SIZE,
    SHADER_UV_OFFSET,
    SHADER_MOTION_PREV_BOX,
    SHADER_MOTION_CURR_BOX,
    SHADER_MOTION_SOURCE_BOX,
    SHADER_MOTION_SOURCE_TEX_ORIGIN,
    SHADER_MOTION_SOURCE_TEX_SIZE,
    SHADER_MOTION_SAMPLES,
    SHADER_BLUR_ALPHA_MATTE,
    SHADER_BLUR_ALPHA,
    SHADER_TONEMAP_MODE,
    SHADER_GLASS_REFRACTION,
    SHADER_GLASS_SIZE,
    SHADER_GLASS_ROUGHNESS,
    SHADER_GLASS_POSITION,
    SHADER_DROPS_POSITION,
    SHADER_SHARP_TEX,
    SHADER_RIPPLE_COUNT,
    SHADER_RIPPLE_IMPULSES,
    SHADER_RIPPLE_PARAMS,
    SHADER_WATER_ENABLED,
    SHADER_WATER_STATE_TEX,
    SHADER_WATER_TEXEL_SIZE,
    SHADER_WATER_EXTENT,
    SHADER_WATER_REFRACTION,
    SHADER_WATER_PARAMS,
    SHADER_WATER_IMPULSE_COUNT,
    SHADER_WATER_IMPULSES,
    SHADER_FLUIDJAR_PARTICLE_TEX,
    SHADER_FLUIDJAR_GRAPH_TEX,
    SHADER_FLUIDJAR_TRACKING_TEX,
    SHADER_FLUIDJAR_VISUAL_TEX,
    SHADER_FLUIDJAR_RESOLUTION,
    SHADER_FLUIDJAR_GRID_SIZE,
    SHADER_FLUIDJAR_PARTICLE_COUNT,
    SHADER_FLUIDJAR_FRAME,
    SHADER_FLUIDJAR_DT,
    SHADER_FLUIDJAR_MASS,
    SHADER_FLUIDJAR_OLD_RESOLUTION,
    SHADER_FLUIDJAR_OLD_GRID_SIZE,
    SHADER_FLUIDJAR_OLD_PARTICLE_COUNT,
    SHADER_FLUIDJAR_TRANSFORM,
    SHADER_FLUIDJAR_VELOCITY_SCALE,
    SHADER_FLUIDJAR_WALL_VELOCITIES,
    SHADER_FLUIDJAR_HISTORY_TEX,
    SHADER_FLUIDJAR_HISTORY_TRANSFORM,
    SHADER_FLUIDJAR_HISTORY_FALLBACK,
    SHADER_FLUIDJAR_EXTENT,
    SHADER_FLUIDJAR_OUTPUT_TRANSFORM,
    SHADER_FLUIDJAR_OUTPUT_OFFSET,
    SHADER_FLUIDJAR_LOGICAL_SIZE,
    SHADER_FLUIDJAR_COLOR,
    SHADER_FLUIDJAR_REFRACTION,
    SHADER_FLUIDJAR_TRANSFER_FUNCTION,
    SHADER_FLUIDJAR_VISUAL_RESPONSE,
    SHADER_FLUIDJAR_STRENGTH,
    SHADER_FLUIDJAR_TURBULENCE,
    SHADER_FLUIDJAR_DISTORTION,
    SHADER_FLUIDJAR_ENABLED,
    SHADER_ACRYLIC_ENABLED,
    SHADER_ACRYLIC_EXTENT,
    SHADER_ACRYLIC_RADIUS,
    SHADER_ACRYLIC_ROUNDING_POWER,
    SHADER_ACRYLIC_REFRACTION,
    SHADER_ACRYLIC_BULB,
    SHADER_ACRYLIC_CLARITY,
    SHADER_ACRYLIC_ABERRATION,
    SHADER_ACRYLIC_TINT,
    SHADER_ACRYLIC_STRENGTH,
    SHADER_ACRYLIC_TRANSFER_FUNCTION,
    SHADER_ACRYLIC_LUMINANCE_SCALE,
    SHADER_AURORA_INTENSITY,
    SHADER_AURORA_COLOR1,
    SHADER_AURORA_COLOR2,
    SHADER_AURORA_TRANSFER_FUNCTION,
    SHADER_HAZE_INTENSITY,
    SHADER_HAZE_IRIDESCENCE,
    SHADER_HAZE_TRANSFER_FUNCTION,

    SHADER_LAST,
};

class CShader {
  public:
    CShader();
    ~CShader();

    bool   createProgram(const std::string& vert, const std::string& frag, bool dynamic = false, bool silent = false);
    void   setUniformInt(eShaderUniform location, GLint v0);
    void   setUniformFloat(eShaderUniform location, GLfloat v0);
    void   setUniformFloat2(eShaderUniform location, GLfloat v0, GLfloat v1);
    void   setUniformFloat3(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2);
    void   setUniformFloat4(eShaderUniform location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void   setUniformMatrix3fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 9> value);
    void   setUniformMatrix4x2fv(eShaderUniform location, GLsizei count, GLboolean transpose, std::array<GLfloat, 8> value);
    void   setUniform1fv(eShaderUniform location, GLsizei count, const std::vector<float>& value);
    void   setUniform2fv(eShaderUniform location, GLsizei count, const std::vector<float>& value);
    void   setUniform4fv(eShaderUniform location, GLsizei count, const std::vector<float>& value);
    void   destroy();
    GLuint program() const;
    GLint  getUniformLocation(eShaderUniform location) const;
    int    getInitialTime() const;
    void   setInitialTime(int time);

  private:
    GLuint                         m_program     = 0;
    float                          m_initialTime = 0;
    std::array<GLint, SHADER_LAST> m_uniformLocations;

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

    void   logShaderError(const GLuint&, bool program = false, bool silent = false);
    GLuint compileShader(const GLuint&, std::string, bool dynamic = false, bool silent = false);
    void   getUniformLocations();
    void   createVao();
    void   setUniformfv(eShaderUniform location, GLsizei count, const std::vector<float>& value, GLsizei vec_size);
};
