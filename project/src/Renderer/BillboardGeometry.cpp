#include "Renderer/BillboardGeometry.h"
#include <glad/glad.h>

void BillboardGeometry::Init()
{
    if (quadVAO != 0) return;

    // Two triangles, corner in [-0.5, 0.5], UV in [0, 1]. Interleaved as
    // (posX, posY, u, v). UV origin bottom-left; y flipped so v=0 is bottom.
    const float vertices[] = {
        //  pos          uv
        -0.5f, -0.5f,   0.0f, 0.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
         0.5f,  0.5f,   1.0f, 1.0f,

        -0.5f, -0.5f,   0.0f, 0.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
        -0.5f,  0.5f,   0.0f, 1.0f,
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void BillboardGeometry::Shutdown()
{
    if (quadVAO != 0) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO != 0) glDeleteBuffers(1, &quadVBO);
    quadVAO = 0;
    quadVBO = 0;
}

void BillboardGeometry::Bind()
{
    glBindVertexArray(quadVAO);
}

void BillboardGeometry::Unbind()
{
    glBindVertexArray(0);
}
