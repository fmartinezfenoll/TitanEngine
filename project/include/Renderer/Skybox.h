#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

class CubemapTexture;

class Skybox
{
public:
    explicit Skybox(std::shared_ptr<CubemapTexture> cubemap, const std::string& name = "");
    ~Skybox() = default;

    void Draw(const glm::mat4& view, const glm::mat4& projection) const;

    const std::string& GetName() const { return name; }
    std::shared_ptr<CubemapTexture> GetCubemap() const { return cubemap; }

    static void InitSharedGeometry();
    static void ShutdownSharedGeometry();

private:
    std::shared_ptr<CubemapTexture> cubemap;
    std::string name;

    static inline unsigned int CubeVAO = 0;
    static inline unsigned int CubeVBO = 0;
};
