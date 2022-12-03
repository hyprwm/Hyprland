#include "Shader.hpp"

GLint CShader::getUniformLocation(const std::string& unif) {
    const auto itpos = m_muUniforms.find(unif);

    if (itpos == m_muUniforms.end()) {
        const auto unifLoc = glGetUniformLocation(program, unif.c_str());
        m_muUniforms[unif] = unifLoc;
        return unifLoc;
    }

    return itpos->second;
}

CShader::~CShader() {
    // destroy shader
    destroy();
}

void CShader::destroy() {
    glDeleteProgram(program);

    program = 0;
}