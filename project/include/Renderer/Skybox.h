#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

class CubemapTexture;

class Skybox
{
public:
    explicit Skybox(std::shared_ptr<CubemapTexture> cubemap, const std::string& name = "");
    ~Skybox();

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    void Draw(const glm::mat4& view, const glm::mat4& projection) const;

    const std::string& GetName() const { return name; }
    std::shared_ptr<CubemapTexture> GetCubemap() const { return cubemap; }

    // Bakes the irradiance and prefiltered environment cubemaps from the
    // source cubemap, once. No-op on subsequent calls.
    void EnsureIBLGenerated();

    bool IsIBLGenerated() const { return iblGenerated; }
    unsigned int GetIrradianceMap() const { return irradianceMapID; }
    unsigned int GetPrefilterMap() const { return prefilterMapID; }

    static void InitSharedGeometry();
    static void ShutdownSharedGeometry();

    static constexpr int kIrradianceResolution = 32;
    static constexpr int kPrefilterResolution = 128;
    static constexpr int kPrefilterMipLevels = 5;

private:
    std::shared_ptr<CubemapTexture> cubemap;
    std::string name;

    unsigned int irradianceMapID = 0;
    unsigned int prefilterMapID = 0;
    bool iblGenerated = false;

    void CreateIrradianceStorage();
    void CreatePrefilterStorage();
    void BakeIrradiance();
    void BakePrefilter();

    static inline unsigned int CubeVAO = 0;
    static inline unsigned int CubeVBO = 0;
};
