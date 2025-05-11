#include "Shader.hpp"
#include "render/OpenGL.hpp"

SShader::~SShader() {
    destroy();
}

void SShader::createVao() {
    glGenVertexArrays(1, &shaderVao);
    glBindVertexArray(shaderVao);

    if (posAttrib != -1) {
        glGenBuffers(1, &shaderVboPos);
        glBindBuffer(GL_ARRAY_BUFFER, shaderVboPos);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), fullVerts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    // UV VBO (dynamic, may be updated per frame)
    if (texAttrib != -1) {
        glGenBuffers(1, &shaderVboUv);
        glBindBuffer(GL_ARRAY_BUFFER, shaderVboUv);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), fullVerts, GL_DYNAMIC_DRAW); // Initial dummy UVs
        glEnableVertexAttribArray(texAttrib);
        glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SShader::destroy() {
    if (program == 0)
        return;

    if (shaderVao)
        glDeleteVertexArrays(1, &shaderVao);

    if (shaderVboPos)
        glDeleteBuffers(1, &shaderVboPos);

    if (shaderVboUv)
        glDeleteBuffers(1, &shaderVboUv);

    glDeleteProgram(program);
    program = 0;
}
