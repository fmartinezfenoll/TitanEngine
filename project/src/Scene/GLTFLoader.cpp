#include "Scene/GLTFLoader.h"
#include "Scene/TNode.h"
#include "Scene/MeshEntity.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/Texture.h"
#include "Core/Log.h"

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>

namespace fs = std::filesystem;

namespace {

Transform GetNodeTransform(const tinygltf::Node& node)
{
    Transform transform;

    if (!node.matrix.empty())
    {
        glm::mat4 m;
        for (int i = 0; i < 16; ++i)
            m[i / 4][i % 4] = static_cast<float>(node.matrix[i]);

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat rotation;
        glm::decompose(m, transform.scale, rotation, transform.position, skew, perspective);
        transform.rotation = glm::degrees(glm::eulerAngles(rotation));
        return transform;
    }

    if (node.translation.size() == 3)
        transform.position = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);

    if (node.rotation.size() == 4)
    {
        glm::quat q(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
                    static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
        transform.rotation = glm::degrees(glm::eulerAngles(q));
    }

    if (node.scale.size() == 3)
        transform.scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);

    return transform;
}

template <typename T>
const T* GetAccessorData(const tinygltf::Model& model, int accessorIndex)
{
    const auto& accessor = model.accessors[accessorIndex];
    const auto& view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    return reinterpret_cast<const T*>(&buffer.data[view.byteOffset + accessor.byteOffset]);
}

std::shared_ptr<Texture> LoadGltfTexture(const tinygltf::Model& model, int textureIndex,
                                          const std::string& cacheKey, const std::string& baseDir)
{
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
        return nullptr;

    const auto& tex = model.textures[textureIndex];
    if (tex.source < 0 || tex.source >= static_cast<int>(model.images.size()))
        return nullptr;

    if (auto existing = ResourceManager::GetTexture(cacheKey))
        return existing;

    const auto& image = model.images[tex.source];

    if (!image.uri.empty())
    {
        std::string path = baseDir.empty() ? image.uri : baseDir + "/" + image.uri;
        return ResourceManager::LoadTexture(cacheKey, path);
    }

    if (!image.image.empty())
    {
        return ResourceManager::LoadTextureFromMemory(cacheKey, image.image.data(),
                                                        static_cast<int>(image.image.size()));
    }

    if (image.bufferView >= 0)
    {
        const auto& view = model.bufferViews[image.bufferView];
        const auto& buffer = model.buffers[view.buffer];
        const unsigned char* encoded = &buffer.data[view.byteOffset];
        return ResourceManager::LoadTextureFromMemory(cacheKey, encoded, static_cast<int>(view.byteLength));
    }

    return nullptr;
}

std::shared_ptr<Material> ProcessMaterial(const tinygltf::Model& model, int materialIndex,
                                           const std::string& path, const std::string& baseDir)
{
    std::string matName = path + "_mat_" + std::to_string(materialIndex);
    if (auto existing = ResourceManager::GetMaterial(matName))
        return existing;

    auto shader = ResourceManager::LoadShader("pbr");
    if (!shader)
    {
        Log::Error("GLTFLoader: pbr shader not found");
        return nullptr;
    }

    auto material = std::make_shared<Material>(shader);

    if (materialIndex >= 0 && materialIndex < static_cast<int>(model.materials.size()))
    {
        const auto& mat = model.materials[materialIndex];
        const auto& pbr = mat.pbrMetallicRoughness;

        if (pbr.baseColorFactor.size() == 4)
        {
            material->baseColor = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1],
                                             pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
        }

        material->albedo = LoadGltfTexture(model, pbr.baseColorTexture.index,
                                            matName + "_albedo", baseDir);
        material->normal = LoadGltfTexture(model, mat.normalTexture.index,
                                            matName + "_normal", baseDir);
        material->metallicRoughness = LoadGltfTexture(model, pbr.metallicRoughnessTexture.index,
                                                        matName + "_mr", baseDir);
    }

    ResourceManager::AddMaterial(matName, material);
    return material;
}

AABB* ComputeAABB(const std::vector<MeshVertex>& vertices)
{
    if (vertices.empty()) return nullptr;

    glm::vec3 min = vertices[0].position;
    glm::vec3 max = vertices[0].position;
    for (const auto& v : vertices)
    {
        min = glm::min(min, v.position);
        max = glm::max(max, v.position);
    }
    return new AABB(min, max);
}

TNode* ProcessMesh(const tinygltf::Primitive& primitive, const tinygltf::Model& model,
                    const std::string& path, const std::string& baseDir)
{
    if (primitive.attributes.find("POSITION") == primitive.attributes.end())
        return nullptr;

    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
    const float* positions = GetAccessorData<float>(model, primitive.attributes.at("POSITION"));

    vertices.resize(posAccessor.count);
    for (size_t i = 0; i < posAccessor.count; ++i)
        vertices[i].position = glm::vec3(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);

    if (primitive.attributes.count("NORMAL"))
    {
        const float* normals = GetAccessorData<float>(model, primitive.attributes.at("NORMAL"));
        for (size_t i = 0; i < posAccessor.count; ++i)
            vertices[i].normal = glm::vec3(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
    }

    if (primitive.attributes.count("TEXCOORD_0"))
    {
        const float* uvs = GetAccessorData<float>(model, primitive.attributes.at("TEXCOORD_0"));
        for (size_t i = 0; i < posAccessor.count; ++i)
            vertices[i].uv = glm::vec2(uvs[i * 2], uvs[i * 2 + 1]);
    }

    if (primitive.indices >= 0)
    {
        const auto& accessor = model.accessors[primitive.indices];
        const auto& view = model.bufferViews[accessor.bufferView];
        const unsigned char* data = &model.buffers[view.buffer].data[view.byteOffset + accessor.byteOffset];

        indices.reserve(accessor.count);
        for (size_t i = 0; i < accessor.count; ++i)
        {
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                indices.push_back(reinterpret_cast<const uint16_t*>(data)[i]);
            else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                indices.push_back(reinterpret_cast<const uint32_t*>(data)[i]);
            else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                indices.push_back(data[i]);
        }
    }

    if (indices.empty())
        return nullptr;

    auto material = ProcessMaterial(model, primitive.material, path, baseDir);
    if (!material)
        return nullptr;

    MeshEntity* mesh = new MeshEntity(vertices, indices, material);
    TNode* node = new TNode(mesh, ComputeAABB(vertices));
    return node;
}

std::vector<TNode*> ProcessNode(const tinygltf::Node& gltfNode, const tinygltf::Model& model,
                                 const std::string& path, const std::string& baseDir)
{
    std::vector<TNode*> result;

    TNode* container = new TNode(nullptr, nullptr, gltfNode.name);
    container->transform = GetNodeTransform(gltfNode);

    if (gltfNode.mesh >= 0 && gltfNode.mesh < static_cast<int>(model.meshes.size()))
    {
        const auto& mesh = model.meshes[gltfNode.mesh];
        for (const auto& primitive : mesh.primitives)
        {
            TNode* meshNode = ProcessMesh(primitive, model, path, baseDir);
            if (meshNode)
                container->addChild(meshNode);
        }
    }

    for (int childIndex : gltfNode.children)
    {
        if (childIndex < 0 || childIndex >= static_cast<int>(model.nodes.size()))
            continue;
        auto childNodes = ProcessNode(model.nodes[childIndex], model, path, baseDir);
        for (TNode* child : childNodes)
            container->addChild(child);
    }

    result.push_back(container);
    return result;
}

} // namespace

std::vector<TNode*> GLTFLoader::LoadModel(const std::string& path)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool isBinary = path.size() >= 4 && path.substr(path.size() - 4) == ".glb";
    bool ok = isBinary
        ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
        : loader.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!warn.empty()) Log::Error("GLTFLoader warning: " + warn);
    if (!err.empty()) Log::Error("GLTFLoader error: " + err);

    if (!ok || model.meshes.empty())
    {
        Log::Error("GLTFLoader: failed to load " + path);
        return {};
    }

    std::string baseDir = fs::path(path).parent_path().string();

    std::vector<TNode*> nodes;
    const auto& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
    for (int nodeIndex : scene.nodes)
    {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size()))
            continue;
        auto processed = ProcessNode(model.nodes[nodeIndex], model, path, baseDir);
        nodes.insert(nodes.end(), processed.begin(), processed.end());
    }

    return nodes;
}
