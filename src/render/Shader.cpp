#include "Shader.hpp"

SShader::~SShader() {
    destroy();
}

void SShader::destroy() {
    if (program == 0)
        return;

    glDeleteProgram(program);

    program = 0;
}
