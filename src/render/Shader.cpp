#include "Shader.hpp"

CShader::~CShader() {
    destroy();
}

void CShader::destroy() {
    if (program == 0)
        return;

    glDeleteProgram(program);

    program = 0;
}
