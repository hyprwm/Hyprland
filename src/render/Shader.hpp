#pragma once

#include "../defines.hpp"
#include <unordered_map>

struct SShader {
    ~SShader();

    GLuint  program           = 0;
    GLint   proj              = -1;
    GLint   color             = -1;
    GLint   alphaMatte        = -1;
    GLint   texType           = -1;
    GLint   skipCM            = -1;
    GLint   sourceTF          = -1;
    GLint   targetTF          = -1;
    GLint   srcTFRange        = -1;
    GLint   dstTFRange        = -1;
    GLint   targetPrimaries   = -1;
    GLint   maxLuminance      = -1;
    GLint   dstMaxLuminance   = -1;
    GLint   dstRefLuminance   = -1;
    GLint   sdrSaturation     = -1; // sdr -> hdr saturation
    GLint   sdrBrightness     = -1; // sdr -> hdr brightness multiplier
    GLint   convertMatrix     = -1;
    GLint   tex               = -1;
    GLint   alpha             = -1;
    GLint   posAttrib         = -1;
    GLint   texAttrib         = -1;
    GLint   matteTexAttrib    = -1;
    GLint   discardOpaque     = -1;
    GLint   discardAlpha      = -1;
    GLfloat discardAlphaValue = -1;

    GLuint  shaderVao    = 0;
    GLuint  shaderVboPos = 0;
    GLuint  shaderVboUv  = 0;

    GLint   topLeft               = -1;
    GLint   bottomRight           = -1;
    GLint   fullSize              = -1;
    GLint   fullSizeUntransformed = -1;
    GLint   radius                = -1;
    GLint   radiusOuter           = -1;
    GLfloat roundingPower         = -1;

    GLint   thick = -1;

    GLint   halfpixel = -1;

    GLint   range         = -1;
    GLint   shadowPower   = -1;
    GLint   useAlphaMatte = -1; // always inverted

    GLint   applyTint = -1;
    GLint   tint      = -1;

    GLint   gradient        = -1;
    GLint   gradientLength  = -1;
    GLint   angle           = -1;
    GLint   gradient2       = -1;
    GLint   gradient2Length = -1;
    GLint   angle2          = -1;
    GLint   gradientLerp    = -1;

    float   initialTime = 0;
    GLint   time        = -1;
    GLint   distort     = -1;
    GLint   wl_output   = -1;

    // Blur prepare
    GLint contrast = -1;

    // Blur
    GLint passes            = -1; // Used by `vibrancy`
    GLint vibrancy          = -1;
    GLint vibrancy_darkness = -1;

    // Blur finish
    GLint                                             brightness = -1;
    GLint                                             noise      = -1;

    std::unordered_map<GLint, GLint>                  uniformStatusInt;
    std::unordered_map<GLint, GLfloat>                uniformStatusFloat;
    std::unordered_map<GLint, std::array<GLfloat, 2>> uniformStatusFloat2;
    std::unordered_map<GLint, std::array<GLfloat, 3>> uniformStatusFloat3;
    std::unordered_map<GLint, std::array<GLfloat, 4>> uniformStatusFloat4;

    struct SUniformMatrix3Data {
        GLsizei                count     = 0;
        GLboolean              transpose = false;
        std::array<GLfloat, 9> value     = {};
    };
    std::unordered_map<GLint, SUniformMatrix3Data> uniformStatusMatrix3fv;

    struct SUniformMatrix4Data {
        GLsizei                count     = 0;
        GLboolean              transpose = false;
        std::array<GLfloat, 8> value     = {};
    };
    std::unordered_map<GLint, SUniformMatrix4Data> uniformStatusMatrix4x2fv;

    struct SUniform4Data {
        GLsizei            count = 0;
        std::vector<float> value;
    };
    std::unordered_map<GLint, SUniform4Data> uniformStatus4fv;

    void                                     createVao();
    void                                     setUniformInt(GLint location, GLint v0);
    void                                     setUniformFloat(GLint location, GLfloat v0);
    void                                     setUniformFloat2(GLint location, GLfloat v0, GLfloat v1);
    void                                     setUniformFloat3(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void                                     setUniformFloat4(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void                                     setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, std::array<GLfloat, 9> value);
    void                                     setUniformMatrix42xfv(GLint location, GLsizei count, GLboolean transpose, std::array<GLfloat, 8> value);
    void                                     setUniform4fv(GLint location, GLsizei count, std::vector<float> value);
    void                                     destroy();
};
