#pragma once

#include "../defines.hpp"
#include <unordered_map>

class CShader {
  public:
    ~CShader();

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
    GLint brightness = -1;
    GLint noise      = -1;

    void  destroy();

  private:
    std::unordered_map<std::string, GLint> m_muUniforms;
};
