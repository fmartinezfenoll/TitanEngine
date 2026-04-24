#include "Scene/SimpleEntities.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

// ============= TRIANGLE =============
static unsigned int triangleVAO = 0;
static unsigned int triangleVBO = 0;

void initTriangleGeometry() {
    if (triangleVAO != 0) return;

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    glGenVertexArrays(1, &triangleVAO);
    glGenBuffers(1, &triangleVBO);

    glBindVertexArray(triangleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, triangleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void TriangleEntity::draw(const glm::mat4& modelMatrix) {
    initTriangleGeometry();

    auto shader = ResourceManager::LoadShader("basic");
    if (!shader) return;

    shader->Bind();
    shader->SetMat4("projection", glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f));
    shader->SetMat4("view", glm::mat4(1.0f));
    shader->SetMat4("model", modelMatrix);

    glBindVertexArray(triangleVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// ============= SQUARE =============
static unsigned int squareVAO = 0;
static unsigned int squareVBO = 0;
static unsigned int squareEBO = 0;

void initSquareGeometry() {
    if (squareVAO != 0) return;

    float vertices[] = {
        -0.5f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    glGenVertexArrays(1, &squareVAO);
    glGenBuffers(1, &squareVBO);
    glGenBuffers(1, &squareEBO);

    glBindVertexArray(squareVAO);
    glBindBuffer(GL_ARRAY_BUFFER, squareVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, squareEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void SquareEntity::draw(const glm::mat4& modelMatrix) {
    initSquareGeometry();

    auto shader = ResourceManager::LoadShader("basic");
    if (!shader) return;

    shader->Bind();
    shader->SetMat4("projection", glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f));
    shader->SetMat4("view", glm::mat4(1.0f));
    shader->SetMat4("model", modelMatrix);

    glBindVertexArray(squareVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
