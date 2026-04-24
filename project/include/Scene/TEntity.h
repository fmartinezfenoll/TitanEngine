#pragma once
#include <glm/glm.hpp>

class TEntity {
public:
    virtual ~TEntity() = default;
    virtual void draw(const glm::mat4& modelMatrix) = 0;
    virtual void update(float deltaTime) {}
};
