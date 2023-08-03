#pragma once

#include "../defines.hpp"
#include <unordered_map>

class CShader {
  public:
    ~CShader();

    GLuint  program           = 0;
    GLint   proj              = -1;
    GLint   color             = -1;
    GLint   tex               = -1;
    GLint   alpha             = -1;
    GLint   posAttrib         = -1;
    GLint   texAttrib         = -1;
    GLint   discardOpaque     = -1;
    GLint   discardAlpha      = -1;
    GLfloat discardAlphaValue = -1;

    GLint   topLeft               = -1;
    GLint   bottomRight           = -1;
    GLint   fullSize              = -1;
    GLint   fullSizeUntransformed = -1;
    GLint   radius                = -1;
    GLint   primitiveMultisample  = -1;

    GLint   thick = -1;

    GLint   halfpixel = -1;

    GLint   range       = -1;
    GLint   shadowPower = -1;

    GLint   applyTint = -1;
    GLint   tint      = -1;

    GLint   gradient       = -1;
    GLint   gradientLength = -1;
    GLint   angle          = -1;

    GLint   time    = -1;
    GLint   distort = -1;
    GLint   output  = -1;

    GLint   noise  = -1;
    GLint   contrast  = -1;
    GLint   brightness  = -1;

    GLint   getUniformLocation(const std::string&);

    void    destroy();

  private:
    std::unordered_map<std::string, GLint> m_muUniforms;
};