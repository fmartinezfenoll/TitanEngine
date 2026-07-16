#include "Scene/MeshPrimitives.h"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>

namespace MeshPrimitives {

void Sphere(std::vector<MeshVertex>& outVertices, std::vector<uint32_t>& outIndices,
            float radius, int segments, int rings) {
    outVertices.clear();
    outIndices.clear();
    segments = std::max(segments, 3);
    rings = std::max(rings, 2);

    const float PI = glm::pi<float>();
    for (int y = 0; y <= rings; ++y) {
        float v = static_cast<float>(y) / rings;
        float phi = v * PI;              // 0..PI (top to bottom)
        for (int x = 0; x <= segments; ++x) {
            float u = static_cast<float>(x) / segments;
            float theta = u * 2.0f * PI; // 0..2PI around

            glm::vec3 normal(
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta));

            MeshVertex vert;
            vert.position = normal * radius;
            vert.normal = normal;
            vert.uv = glm::vec2(u, 1.0f - v);
            outVertices.push_back(vert);
        }
    }

    int stride = segments + 1;
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            uint32_t i0 = y * stride + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + stride;
            uint32_t i3 = i2 + 1;
            outIndices.insert(outIndices.end(), { i0, i2, i1, i1, i2, i3 });
        }
    }
}

void Plane(std::vector<MeshVertex>& outVertices, std::vector<uint32_t>& outIndices,
           float size, int subdivisions) {
    outVertices.clear();
    outIndices.clear();
    subdivisions = std::max(subdivisions, 1);

    float half = size * 0.5f;
    for (int z = 0; z <= subdivisions; ++z) {
        float fz = static_cast<float>(z) / subdivisions;
        for (int x = 0; x <= subdivisions; ++x) {
            float fx = static_cast<float>(x) / subdivisions;
            MeshVertex vert;
            vert.position = glm::vec3(-half + fx * size, 0.0f, -half + fz * size);
            vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vert.uv = glm::vec2(fx, fz);
            outVertices.push_back(vert);
        }
    }

    int stride = subdivisions + 1;
    for (int z = 0; z < subdivisions; ++z) {
        for (int x = 0; x < subdivisions; ++x) {
            uint32_t i0 = z * stride + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + stride;
            uint32_t i3 = i2 + 1;
            outIndices.insert(outIndices.end(), { i0, i2, i1, i1, i2, i3 });
        }
    }
}

void Cylinder(std::vector<MeshVertex>& outVertices, std::vector<uint32_t>& outIndices,
              float radius, float height, int segments) {
    outVertices.clear();
    outIndices.clear();
    segments = std::max(segments, 3);

    const float PI = glm::pi<float>();
    float halfH = height * 0.5f;

    // Side wall: two rings of vertices (bottom, top) with outward normals.
    for (int y = 0; y <= 1; ++y) {
        float py = (y == 0) ? -halfH : halfH;
        for (int x = 0; x <= segments; ++x) {
            float u = static_cast<float>(x) / segments;
            float theta = u * 2.0f * PI;
            glm::vec3 dir(std::cos(theta), 0.0f, std::sin(theta));
            MeshVertex vert;
            vert.position = glm::vec3(dir.x * radius, py, dir.z * radius);
            vert.normal = dir;
            vert.uv = glm::vec2(u, static_cast<float>(y));
            outVertices.push_back(vert);
        }
    }
    int stride = segments + 1;
    for (int x = 0; x < segments; ++x) {
        uint32_t i0 = x, i1 = x + 1, i2 = stride + x, i3 = stride + x + 1;
        outIndices.insert(outIndices.end(), { i0, i2, i1, i1, i2, i3 });
    }

    // Caps: a center vertex + a fan for each. Normals point along +/-Y.
    auto addCap = [&](float py, float ny) {
        uint32_t center = static_cast<uint32_t>(outVertices.size());
        MeshVertex c;
        c.position = glm::vec3(0.0f, py, 0.0f);
        c.normal = glm::vec3(0.0f, ny, 0.0f);
        c.uv = glm::vec2(0.5f, 0.5f);
        outVertices.push_back(c);

        uint32_t ringStart = static_cast<uint32_t>(outVertices.size());
        for (int x = 0; x <= segments; ++x) {
            float u = static_cast<float>(x) / segments;
            float theta = u * 2.0f * PI;
            glm::vec3 dir(std::cos(theta), 0.0f, std::sin(theta));
            MeshVertex vert;
            vert.position = glm::vec3(dir.x * radius, py, dir.z * radius);
            vert.normal = glm::vec3(0.0f, ny, 0.0f);
            vert.uv = glm::vec2(dir.x * 0.5f + 0.5f, dir.z * 0.5f + 0.5f);
            outVertices.push_back(vert);
        }
        for (int x = 0; x < segments; ++x) {
            uint32_t a = ringStart + x, b = ringStart + x + 1;
            // Wind so the cap faces outward along ny.
            if (ny > 0.0f) outIndices.insert(outIndices.end(), { center, a, b });
            else           outIndices.insert(outIndices.end(), { center, b, a });
        }
    };
    addCap(halfH, 1.0f);
    addCap(-halfH, -1.0f);
}

void Cone(std::vector<MeshVertex>& outVertices, std::vector<uint32_t>& outIndices,
          float radius, float height, int segments) {
    outVertices.clear();
    outIndices.clear();
    segments = std::max(segments, 3);

    const float PI = glm::pi<float>();
    float halfH = height * 0.5f;
    glm::vec3 apex(0.0f, halfH, 0.0f);

    // Side: one apex vertex per segment (duplicated so each face gets a proper
    // slanted normal) plus the base ring.
    float slant = std::sqrt(radius * radius + height * height);
    for (int x = 0; x < segments; ++x) {
        float u0 = static_cast<float>(x) / segments;
        float u1 = static_cast<float>(x + 1) / segments;
        float t0 = u0 * 2.0f * PI;
        float t1 = u1 * 2.0f * PI;
        glm::vec3 b0(std::cos(t0) * radius, -halfH, std::sin(t0) * radius);
        glm::vec3 b1(std::cos(t1) * radius, -halfH, std::sin(t1) * radius);

        // Face normal: slanted outward. Approximate per-vertex with the radial
        // direction tilted by the slope (height/slant vertical, radius/slant out).
        auto slantNormal = [&](const glm::vec3& base) {
            glm::vec3 radial = glm::normalize(glm::vec3(base.x, 0.0f, base.z));
            return glm::normalize(glm::vec3(radial.x * height / slant, radius / slant, radial.z * height / slant));
        };

        uint32_t start = static_cast<uint32_t>(outVertices.size());
        MeshVertex va; va.position = apex; va.normal = slantNormal((b0 + b1) * 0.5f); va.uv = glm::vec2((u0 + u1) * 0.5f, 1.0f);
        MeshVertex v0; v0.position = b0; v0.normal = slantNormal(b0); v0.uv = glm::vec2(u0, 0.0f);
        MeshVertex v1; v1.position = b1; v1.normal = slantNormal(b1); v1.uv = glm::vec2(u1, 0.0f);
        outVertices.push_back(va);
        outVertices.push_back(v0);
        outVertices.push_back(v1);
        outIndices.insert(outIndices.end(), { start, start + 1, start + 2 });
    }

    // Bottom cap.
    uint32_t center = static_cast<uint32_t>(outVertices.size());
    MeshVertex c; c.position = glm::vec3(0.0f, -halfH, 0.0f); c.normal = glm::vec3(0.0f, -1.0f, 0.0f); c.uv = glm::vec2(0.5f, 0.5f);
    outVertices.push_back(c);
    uint32_t ringStart = static_cast<uint32_t>(outVertices.size());
    for (int x = 0; x <= segments; ++x) {
        float u = static_cast<float>(x) / segments;
        float theta = u * 2.0f * PI;
        glm::vec3 dir(std::cos(theta), 0.0f, std::sin(theta));
        MeshVertex vert;
        vert.position = glm::vec3(dir.x * radius, -halfH, dir.z * radius);
        vert.normal = glm::vec3(0.0f, -1.0f, 0.0f);
        vert.uv = glm::vec2(dir.x * 0.5f + 0.5f, dir.z * 0.5f + 0.5f);
        outVertices.push_back(vert);
    }
    for (int x = 0; x < segments; ++x) {
        uint32_t a = ringStart + x, b = ringStart + x + 1;
        outIndices.insert(outIndices.end(), { center, b, a }); // faces -Y
    }
}

} // namespace MeshPrimitives
