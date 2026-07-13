#pragma once
#include "Scene/TNode.h"
#include <vector>
#include <memory>

class Skybox;

class Scene {
public:
    Scene() : root(nullptr) {}
    ~Scene();

    void Init();

    TNode* GetRoot() { return root; }
    TNode* CreateNode(BoundingVolume* boundingBox = nullptr);

    void AddNodeToRoot(TNode* node);
    void RemoveNode(TNode* node);

    void Update(float deltaTime);
    void Draw(const Frustum& frustum, const glm::mat4& view, const glm::mat4& projection,
              const std::vector<LightUniformData>& lightUniforms, const ShadowRenderData& shadowData);

    void Clear();

    void RegisterCamera(TNode* cameraNode);
    void UnregisterCamera(TNode* cameraNode);
    const std::vector<TNode*>& GetCameras() const { return cameras; }

    void SetMainCamera(TNode* cameraNode) { mainCamera = cameraNode; }
    TNode* GetMainCamera() const { return mainCamera; }

    void RegisterLight(TNode* lightNode);
    void UnregisterLight(TNode* lightNode);
    const std::vector<TNode*>& GetLights() const { return lights; }

    void SetSkybox(std::shared_ptr<Skybox> newSkybox) { skybox = std::move(newSkybox); }
    Skybox* GetSkybox() const { return skybox.get(); }

    bool IsGridVisible() const { return showGrid; }
    void SetGridVisible(bool visible) { showGrid = visible; }

private:
    TNode* root = nullptr;
    std::vector<TNode*> cameras;
    TNode* mainCamera = nullptr;
    std::vector<TNode*> lights;
    std::shared_ptr<Skybox> skybox;
    bool showGrid = true;
};
