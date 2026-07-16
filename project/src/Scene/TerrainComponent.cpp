#include "Scene/TerrainComponent.h"
#include "Scene/TNode.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/Material.h"
#include "Core/Log.h"
#include <stb_image.h>
#include <vector>
#include <algorithm>
#include <cmath>

TerrainComponent::TerrainComponent(TNode* owner) : owner(owner) {}

void TerrainComponent::Generate() {
    if (!owner) return;

    int res = std::max(resolution, 2);

    // Two height sources: a loaded grayscale heightmap, or -- when none is set
    // -- procedural rolling hills (sum of sines). Both return world-space height.
    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = nullptr;
    if (!heightmapPath.empty()) {
        pixels = stbi_load(heightmapPath.c_str(), &width, &height, &channels, 1);
        if (!pixels) {
            Log::Error("TerrainComponent: failed to load heightmap '" + heightmapPath +
                       "', falling back to procedural terrain");
        }
    }

    auto proceduralHeight = [&](int gx, int gz) -> float {
        float fx = static_cast<float>(gx) * noiseFrequency;
        float fz = static_cast<float>(gz) * noiseFrequency;
        // A few sines at different frequencies for non-repetitive rolling hills,
        // normalized to [0,1] then scaled.
        float h = std::sin(fx) * std::cos(fz)
                + 0.5f * std::sin(fx * 2.3f + 1.7f) * std::cos(fz * 1.9f)
                + 0.25f * std::sin(fx * 4.1f) * std::cos(fz * 3.7f + 0.5f);
        h = (h / 1.75f) * 0.5f + 0.5f; // -> [0,1]
        return h * heightScale;
    };

    auto sampleHeight = [&](int gx, int gz) -> float {
        if (!pixels) return proceduralHeight(gx, gz);
        int px = std::clamp(gx * (width - 1) / (res - 1), 0, width - 1);
        int pz = std::clamp(gz * (height - 1) / (res - 1), 0, height - 1);
        return (pixels[pz * width + px] / 255.0f) * heightScale;
    };

    std::vector<MeshVertex> vertices;
    vertices.reserve(static_cast<size_t>(res) * res);
    float half = size * 0.5f;
    float step = size / (res - 1);

    for (int z = 0; z < res; ++z) {
        for (int x = 0; x < res; ++x) {
            MeshVertex v;
            float y = sampleHeight(x, z);
            v.position = glm::vec3(-half + x * step, y, -half + z * step);

            // Central-difference normal from neighboring heights.
            float hl = sampleHeight(std::max(x - 1, 0), z);
            float hr = sampleHeight(std::min(x + 1, res - 1), z);
            float hd = sampleHeight(x, std::max(z - 1, 0));
            float hu = sampleHeight(x, std::min(z + 1, res - 1));
            v.normal = glm::normalize(glm::vec3(hl - hr, 2.0f * step, hd - hu));

            v.uv = glm::vec2(static_cast<float>(x) / (res - 1), static_cast<float>(z) / (res - 1));
            vertices.push_back(v);
        }
    }

    if (pixels) stbi_image_free(pixels);

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(res - 1) * (res - 1) * 6);
    for (int z = 0; z < res - 1; ++z) {
        for (int x = 0; x < res - 1; ++x) {
            uint32_t i0 = z * res + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + res;
            uint32_t i3 = i2 + 1;
            indices.insert(indices.end(), { i0, i2, i1, i1, i2, i3 });
        }
    }

    // Replace any existing mesh (regeneration) and install the new one.
    owner->RemoveComponent<MeshComponent>();
    auto* mesh = owner->AddComponent<MeshComponent>(vertices, indices);

    glm::vec3 localMin, localMax;
    mesh->GetLocalBounds(localMin, localMax);
    if (owner->boundingBox) { delete owner->boundingBox; owner->boundingBox = nullptr; }
    owner->boundingBox = new AABB(localMin, localMax);

    if (!owner->GetComponent<MaterialComponent>()) {
        auto material = std::make_shared<Material>(ResourceManager::LoadShader("pbr"));
        material->baseColor = glm::vec4(0.45f, 0.4f, 0.3f, 1.0f);
        owner->AddComponent<MaterialComponent>(material);
    }
}
