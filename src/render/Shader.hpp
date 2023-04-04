#pragma once

#include "../defines.hpp"
#include <unordered_map>

class CShader {
  public:
    ~CShader();

    GLuint program = 0;
    GLint  proj;
    GLint  color;
    GLint  tex;
    GLint  alpha;
    GLint  posAttrib;
    GLint  texAttrib;
    GLint  discardOpaque;
    GLint  discardAlphaZero;

    GLint  topLeft;
    GLint  bottomRight;
    GLint  fullSize;
    GLint  fullSizeUntransformed;
    GLint  radius;
    GLint  primitiveMultisample;

    GLint  thick;

    GLint  halfpixel;

    GLint  range;
    GLint  shadowPower;

    GLint  applyTint;
    GLint  tint;

    GLint  gradient;
    GLint  gradientLength;
    GLint  angle;

    GLint  time;
    GLint  distort;

    GLint  getUniformLocation(const std::string&);

    void   destroy();

  private:
    std::unordered_map<std::string, GLint> m_muUniforms;
};