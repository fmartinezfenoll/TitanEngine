// Builds the built-in demo/test scenes (Triangle, Square, GLTF, Duck,
// Animation Test) the first time the engine runs with no scenes/ directory --
// exercises engine features (glTF loading, skinning, patrol, state machines,
// camera paths, skybox/lighting) without needing hand-authored .scene files.
// Kept separate from Application.cpp so that file stays focused on the
// actual application lifecycle (Init/Run/Update/Shutdown).
#include "Core/Application.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/CubemapTexture.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Scene/SceneSerializer.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/GLTFLoader.h"
#include "Scene/AnimationComponent.h"
#include "Scene/AnimationStateMachine.h"
#include "Scene/PatrolComponent.h"
#include "Scene/CameraPathComponent.h"
#include "Renderer/Skybox.h"
#include "Core/EngineSettings.h"
#include "Core/Log.h"
#include <glm/glm.hpp>
#include <filesystem>

namespace {

TNode* BuildTriangleNode() {
    std::vector<MeshVertex> vertices = {
        { {-0.5f, -0.5f, 0.0f}, {}, {} },
        { { 0.5f, -0.5f, 0.0f}, {}, {} },
        { { 0.0f,  0.5f, 0.0f}, {}, {} },
    };
    std::vector<uint32_t> indices = { 0, 1, 2 };

    auto material = std::make_shared<Material>(ResourceManager::LoadShader("basic"));

    TNode* node = new TNode(nullptr, "Triangle");
    auto* mesh = node->AddComponent<MeshComponent>(vertices, indices);
    node->AddComponent<MaterialComponent>(material);

    glm::vec3 localMin, localMax;
    mesh->GetLocalBounds(localMin, localMax);
    node->boundingBox = new AABB(localMin, localMax);
    return node;
}

TNode* BuildSquareNode() {
    std::vector<MeshVertex> vertices = {
        { {-0.5f,  0.5f, 0.0f}, {}, {} },
        { {-0.5f, -0.5f, 0.0f}, {}, {} },
        { { 0.5f, -0.5f, 0.0f}, {}, {} },
        { { 0.5f,  0.5f, 0.0f}, {}, {} },
    };
    std::vector<uint32_t> indices = { 0, 1, 2, 0, 2, 3 };

    auto material = std::make_shared<Material>(ResourceManager::LoadShader("basic"));

    TNode* node = new TNode(nullptr, "Square");
    auto* mesh = node->AddComponent<MeshComponent>(vertices, indices);
    node->AddComponent<MaterialComponent>(material);

    glm::vec3 localMin, localMax;
    mesh->GetLocalBounds(localMin, localMax);
    node->boundingBox = new AABB(localMin, localMax);
    return node;
}

TNode* BuildGroundPlaneNode(float size) {
    float half = size * 0.5f;
    std::vector<MeshVertex> vertices = {
        { {-half, 0.0f,  half}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
        { { half, 0.0f,  half}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f} },
        { { half, 0.0f, -half}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} },
        { {-half, 0.0f, -half}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f} },
    };
    std::vector<uint32_t> indices = { 0, 1, 2, 0, 2, 3 };

    auto material = std::make_shared<Material>(ResourceManager::LoadShader("pbr"));
    material->baseColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);

    TNode* node = new TNode(nullptr, "Ground");
    auto* mesh = node->AddComponent<MeshComponent>(vertices, indices);
    node->AddComponent<MaterialComponent>(material);

    glm::vec3 localMin, localMax;
    mesh->GetLocalBounds(localMin, localMax);
    node->boundingBox = new AABB(localMin, localMax);
    return node;
}

} // namespace

void Application::SetupScenes()
{
    SceneManager& sm = SceneManager::Instance();

    std::filesystem::path scenesDir("scenes");
    if (std::filesystem::exists(scenesDir) && !std::filesystem::is_empty(scenesDir)) {
        sm.LoadAllScenesFromDirectory("scenes");
    } else {
        std::filesystem::create_directories("scenes");

        Scene* triangleScene = sm.CreateScene("Triangle Scene");
        TNode* triangleNode = BuildTriangleNode();
        triangleNode->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        triangleScene->AddNodeToRoot(triangleNode);
        SceneSerializer::SaveScene(triangleScene, "scenes/Triangle Scene.scene");

        Scene* squareScene = sm.CreateScene("Square Scene");
        TNode* squareNode = BuildSquareNode();
        squareNode->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        squareScene->AddNodeToRoot(squareNode);
        SceneSerializer::SaveScene(squareScene, "scenes/Square Scene.scene");

        sm.LoadScene("Triangle Scene");
    }

    if (!sm.GetScene("GLTF Scene")) {
        Scene* gltfScene = sm.CreateScene("GLTF Scene");
        for (TNode* node : GLTFLoader::LoadModel("resources/models/Box.glb")) {
            gltfScene->AddNodeToRoot(node);
        }
    }

    if (!sm.GetScene("Duck Scene")) {
        Scene* duckScene = sm.CreateScene("Duck Scene");
        for (TNode* node : GLTFLoader::LoadModel("resources/models/Duck.glb")) {
            duckScene->AddNodeToRoot(node);
        }
        duckScene->AddNodeToRoot(BuildGroundPlaneNode(300.0f));
    }

    if (!sm.GetScene("Animation Test")) {
        Scene* animScene = sm.CreateScene("Animation Test");

        TNode* characterRoot = new TNode(nullptr, "CharacterRoot");

        animScene->AddNodeToRoot(characterRoot);

        // AddNodeToRoot only checks the node passed to it, not its descendants --
        // CesiumMan's AnimationComponent lives on the glTF's own root node, which
        // is a CHILD of characterRoot here, so it must be registered explicitly.
        std::vector<TNode*> cesiumManNodes = GLTFLoader::LoadModel("resources/models/CesiumMan.glb");
        for (TNode* node : cesiumManNodes) {
            characterRoot->addChild(node);
            if (auto* anim = node->GetComponent<AnimationComponent>()) {
                if (!anim->GetClips().empty()) {
                    const std::string& clipName = anim->GetClips().front().name;
                    float duration = anim->GetClips().front().duration;

                    // Demo of the state machine + events + blending built this round.
                    // There's no separate "stand still" clip for CesiumMan, so both
                    // states reuse the same walk clip -- enough to exercise the
                    // mechanics (transition + 0.25s blend + event firing) even though
                    // it isn't a real idle pose. Scene::Update feeds "isMoving" from
                    // this node's PARENT PatrolComponent each frame (see Scene.cpp).
                    auto stateMachine = std::make_unique<AnimationStateMachine>();
                    stateMachine->AddState("Idle", clipName, /*loop=*/true);
                    stateMachine->AddState("Walk", clipName, /*loop=*/true);
                    stateMachine->AddTransition("Idle", "Walk", "isMoving", AnimationStateMachine::ConditionOp::Equals, 1.0f, 0.25f);
                    stateMachine->AddTransition("Walk", "Idle", "isMoving", AnimationStateMachine::ConditionOp::Equals, 0.0f, 0.25f);
                    stateMachine->SetInitialState("Idle");
                    anim->SetStateMachine(std::move(stateMachine));

                    anim->AddEvent(clipName, duration * 0.5f, "Footstep");
                    anim->SetEventCallback([](const std::string& eventName) {
                        Log::Info("Animation event fired: " + eventName);
                    });
                }
                animScene->RegisterAnimator(node);
            }
        }

        auto* patrol = characterRoot->AddComponent<PatrolComponent>(characterRoot);
        patrol->SetForwardOffset(180.0f); // CesiumMan's glTF-authored forward is +Z, opposite of what atan2 assumes
        patrol->AddWaypoint(glm::vec3( 5.0f, 0.0f,  5.0f), 1.0f);
        patrol->AddWaypoint(glm::vec3(-5.0f, 0.0f,  5.0f), 1.0f);
        patrol->AddWaypoint(glm::vec3(-5.0f, 0.0f, -5.0f), 1.0f);
        patrol->AddWaypoint(glm::vec3( 5.0f, 0.0f, -5.0f), 1.0f);
        animScene->RegisterAnimator(characterRoot);

        animScene->AddNodeToRoot(BuildGroundPlaneNode(20.0f));

        // Skybox + lighting so the character and ground actually read well
        // instead of the flat ambient-only fallback.
        auto skyboxCubemap = ResourceManager::LoadSkyboxFromFolder("space");
        if (skyboxCubemap) {
            animScene->SetSkybox(std::make_shared<Skybox>(skyboxCubemap, "space"));
        }

        TNode* sunNode = new TNode(nullptr, "Sun");
        auto* sun = sunNode->AddComponent<LightComponent>(sunNode, LightType::Directional);
        sun->intensity = 3.0f;
        sun->color = glm::vec3(1.0f, 0.96f, 0.9f);
        sunNode->transform.rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
        animScene->AddNodeToRoot(sunNode);

        TNode* fillLightNode = new TNode(nullptr, "FillLight");
        auto* fillLight = fillLightNode->AddComponent<LightComponent>(fillLightNode, LightType::Point);
        fillLight->intensity = 2.0f;
        fillLight->range = 25.0f;
        fillLight->color = glm::vec3(0.4f, 0.6f, 1.0f);
        fillLightNode->transform.position = glm::vec3(-6.0f, 4.0f, -6.0f);
        animScene->AddNodeToRoot(fillLightNode);

        TNode* rimLightNode = new TNode(nullptr, "RimLight");
        auto* rimLight = rimLightNode->AddComponent<LightComponent>(rimLightNode, LightType::Point);
        rimLight->intensity = 2.0f;
        rimLight->range = 25.0f;
        rimLight->color = glm::vec3(1.0f, 0.5f, 0.3f);
        rimLightNode->transform.position = glm::vec3(6.0f, 4.0f, 6.0f);
        animScene->AddNodeToRoot(rimLightNode);

        // A scripted camera move (CameraPathComponent): orbits around the
        // patrol area on a smooth Catmull-Rom curve while CesiumMan walks its
        // own waypoint loop below, always looking back at the character's
        // starting position. Press Play in the Inspector (Camera Path
        // section) to see it -- it doesn't auto-play.
        TNode* cameraNode = new TNode(nullptr, "MainCamera");
        CameraComponent* camera = cameraNode->AddComponent<CameraComponent>(cameraNode);
        cameraNode->transform.position = glm::vec3(0.0f, 12.0f, 16.0f);
        camera->pitch = -35.0f;

        auto* cameraPath = cameraNode->AddComponent<CameraPathComponent>(cameraNode);
        glm::vec3 orbitCenter(0.0f, 1.0f, 0.0f);
        cameraPath->AddPoint(glm::vec3(0.0f, 12.0f, 16.0f), orbitCenter, 3.0f, 1.0f);
        cameraPath->AddPoint(glm::vec3(16.0f, 8.0f, 0.0f), orbitCenter, 3.0f, 1.0f);
        cameraPath->AddPoint(glm::vec3(0.0f, 6.0f, -16.0f), orbitCenter, 3.0f, 1.0f);
        cameraPath->AddPoint(glm::vec3(-16.0f, 8.0f, 0.0f), orbitCenter, 3.0f, 1.0f);
        cameraPath->SetLooping(true);
        animScene->AddNodeToRoot(cameraNode);
        animScene->RegisterAnimator(cameraNode);
        animScene->SetMainCamera(cameraNode);
    }

    const std::string& lastActiveScene = EngineSettings::GetLastActiveScene();
    if (!lastActiveScene.empty() && sm.GetScene(lastActiveScene)) {
        sm.LoadScene(lastActiveScene);
    } else {
        sm.LoadScene("Duck Scene");
    }

    for (const auto& [name, scene] : sm.GetAllScenes()) {
        if (!scene->GetMainCamera()) {
            TNode* cameraNode = new TNode(nullptr, "MainCamera");
            CameraComponent* camera = cameraNode->AddComponent<CameraComponent>(cameraNode);

            if (name == "Duck Scene") {
                cameraNode->transform.position = glm::vec3(0.0f, 20.0f, 60.0f);
                camera->farPlane = 1000.0f;
            } else {
                cameraNode->transform.position = glm::vec3(0.0f, 0.0f, 3.0f);
            }

            scene->AddNodeToRoot(cameraNode);
        }

        if (scene->GetLights().empty()) {
            TNode* lightNode = new TNode(nullptr, "DirectionalLight1");
            auto* light = lightNode->AddComponent<LightComponent>(lightNode, LightType::Directional);
            lightNode->transform.rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
            if (name == "Duck Scene") {
                light->castsShadow = true;
            }
            scene->AddNodeToRoot(lightNode);
        }
    }
}
