#pragma once

#include "../defines.hpp"
#include <unordered_map>

class CShader {
public:
    GLuint program;
    GLint proj;
    GLint color;
    GLint tex;
    GLint alpha;
    GLint posAttrib;
    GLint texAttrib;
    GLint discardOpaque;

    GLint getUniformLocation(const std::string&);

private:
    std::unordered_map<std::string, GLint> m_muUniforms;
};