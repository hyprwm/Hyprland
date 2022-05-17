#pragma once

#include "../defines.hpp"

struct SQuad {
    GLuint program;
    GLint proj;
    GLint color;
    GLint posAttrib;
    GLint texAttrib;
};

class CShader {
public:
    GLuint program;
    GLint proj;
    GLint tex;
    GLint alpha;
    GLint posAttrib;
    GLint texAttrib;
    GLint discardOpaque;
};