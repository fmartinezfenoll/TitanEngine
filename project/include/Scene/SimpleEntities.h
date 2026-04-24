#pragma once
#include "Scene/TEntity.h"
#include <glm/glm.hpp>

class TriangleEntity : public TEntity {
public:
    TriangleEntity() = default;
    void draw(const glm::mat4& modelMatrix) override;
    void update(float deltaTime) override {}
};

class SquareEntity : public TEntity {
public:
    SquareEntity() = default;
    void draw(const glm::mat4& modelMatrix) override;
    void update(float deltaTime) override {}
};
