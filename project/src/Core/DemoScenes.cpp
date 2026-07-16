// Builds the built-in demo/test scenes (Triangle, Square, GLTF, Duck,
// Animation Test, VFX Test, Terrain Test) the first time the engine runs with
// no scenes/ directory -- exercises engine features (glTF loading, skinning,
// patrol, state machines, camera paths, skybox/lighting, grass/billboards/
// particles, fog, procedural terrain) without needing hand-authored .scene files.
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
#include "Scene/BillboardComponent.h"
#include "Scene/GrassComponent.h"
#include "Scene/ParticleSystemComponent.h"
#include "Scene/TerrainComponent.h"
#include "Scene/MeshPrimitives.h"
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

// Builds a node with the given geometry, a material using `shaderName`, and a
// base color. Used by the shader-showcase demo scenes below.
TNode* BuildShadedNode(const std::string& name, const std::string& shaderName,
                       const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices,
                       const glm::vec4& color, bool transparent = false) {
    auto material = std::make_shared<Material>(ResourceManager::LoadShader(shaderName));
    material->baseColor = color;
    material->transparent = transparent;

    TNode* node = new TNode(nullptr, name);
    auto* mesh = node->AddComponent<MeshComponent>(vertices, indices);
    node->AddComponent<MaterialComponent>(material);

    glm::vec3 localMin, localMax;
    mesh->GetLocalBounds(localMin, localMax);
    node->boundingBox = new AABB(localMin, localMax);
    return node;
}

// A ground plane using an arbitrary shader (mirrors BuildGroundPlaneNode but
// lets the showcase scenes ground their objects in the same shader style).
TNode* BuildShadedGround(const std::string& shaderName, float size, const glm::vec4& color) {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    MeshPrimitives::Plane(vertices, indices, size, 1);
    return BuildShadedNode("Ground", shaderName, vertices, indices, color);
}

// A varied arrangement of primitives (spheres, cylinders, cones, a big back
// sphere) all using `shaderName` -- shows a shading style across curved and
// faceted surfaces at different scales and positions so lighting reads well.
void AddPrimitiveShowcase(Scene* scene, const std::string& shaderName) {
    std::vector<MeshVertex> v;
    std::vector<uint32_t> idx;

    // Front row: sphere, cylinder, cone.
    MeshPrimitives::Sphere(v, idx, 1.2f, 32, 24);
    TNode* sphere = BuildShadedNode("Sphere", shaderName, v, idx, glm::vec4(0.85f, 0.35f, 0.35f, 1.0f));
    sphere->transform.position = glm::vec3(-3.0f, 1.2f, 0.0f);
    scene->AddNodeToRoot(sphere);

    v.clear(); idx.clear();
    MeshPrimitives::Cylinder(v, idx, 1.0f, 2.4f, 32);
    TNode* cylinder = BuildShadedNode("Cylinder", shaderName, v, idx, glm::vec4(0.4f, 0.7f, 0.45f, 1.0f));
    cylinder->transform.position = glm::vec3(0.0f, 1.2f, 0.0f);
    scene->AddNodeToRoot(cylinder);

    v.clear(); idx.clear();
    MeshPrimitives::Cone(v, idx, 1.1f, 2.4f, 32);
    TNode* cone = BuildShadedNode("Cone", shaderName, v, idx, glm::vec4(0.45f, 0.55f, 0.9f, 1.0f));
    cone->transform.position = glm::vec3(3.0f, 1.2f, 0.0f);
    scene->AddNodeToRoot(cone);

    // A large sphere behind the row -- a big smooth surface makes the lighting
    // gradients (toon bands / dither) very readable.
    v.clear(); idx.clear();
    MeshPrimitives::Sphere(v, idx, 2.6f, 48, 36);
    TNode* bigSphere = BuildShadedNode("Big Sphere", shaderName, v, idx, glm::vec4(0.75f, 0.72f, 0.68f, 1.0f));
    bigSphere->transform.position = glm::vec3(-1.0f, 2.6f, -6.0f);
    scene->AddNodeToRoot(bigSphere);

    // Two small spheres flanking, catching the colored point lights.
    v.clear(); idx.clear();
    MeshPrimitives::Sphere(v, idx, 0.8f, 24, 18);
    TNode* smallLeft = BuildShadedNode("Small Sphere L", shaderName, v, idx, glm::vec4(0.8f, 0.8f, 0.85f, 1.0f));
    smallLeft->transform.position = glm::vec3(-6.0f, 0.8f, -2.0f);
    scene->AddNodeToRoot(smallLeft);

    v.clear(); idx.clear();
    MeshPrimitives::Sphere(v, idx, 0.8f, 24, 18);
    TNode* smallRight = BuildShadedNode("Small Sphere R", shaderName, v, idx, glm::vec4(0.8f, 0.8f, 0.85f, 1.0f));
    smallRight->transform.position = glm::vec3(6.0f, 0.8f, -2.0f);
    scene->AddNodeToRoot(smallRight);

    // A couple of tall pillars for vertical variety and cast-shadow-like banding.
    v.clear(); idx.clear();
    MeshPrimitives::Cylinder(v, idx, 0.5f, 5.0f, 24);
    TNode* pillarL = BuildShadedNode("Pillar L", shaderName, v, idx, glm::vec4(0.6f, 0.6f, 0.65f, 1.0f));
    pillarL->transform.position = glm::vec3(-8.0f, 2.5f, -5.0f);
    scene->AddNodeToRoot(pillarL);

    v.clear(); idx.clear();
    MeshPrimitives::Cylinder(v, idx, 0.5f, 5.0f, 24);
    TNode* pillarR = BuildShadedNode("Pillar R", shaderName, v, idx, glm::vec4(0.6f, 0.6f, 0.65f, 1.0f));
    pillarR->transform.position = glm::vec3(8.0f, 2.5f, -5.0f);
    scene->AddNodeToRoot(pillarR);
}

// Adds a colored point light to `scene` at a position, with a given color,
// intensity and range. Returns the node so callers can tweak it further.
TNode* AddPointLight(Scene* scene, const std::string& name, const glm::vec3& pos,
                     const glm::vec3& color, float intensity, float range) {
    TNode* node = new TNode(nullptr, name);
    auto* light = node->AddComponent<LightComponent>(node, LightType::Point);
    light->color = color;
    light->intensity = intensity;
    light->range = range;
    node->transform.position = pos;
    scene->AddNodeToRoot(node);
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
                    // the nearest ancestor PatrolComponent each frame (see Scene.cpp).
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

    if (!sm.GetScene("VFX Test")) {
        Scene* vfxScene = sm.CreateScene("VFX Test");

        vfxScene->AddNodeToRoot(BuildGroundPlaneNode(40.0f));

        // Grass patch covering most of the ground.
        TNode* grassNode = new TNode(nullptr, "Grass");
        auto* grass = grassNode->AddComponent<GrassComponent>();
        grass->areaSize = glm::vec2(30.0f, 30.0f);
        grass->density = 4000;
        grass->bladeSize = glm::vec2(0.25f, 0.8f);
        grass->windStrength = 0.12f;
        grass->Rebuild();
        vfxScene->AddNodeToRoot(grassNode);

        // Campfire: fire + smoke rising from the same spot.
        TNode* fireNode = new TNode(nullptr, "Campfire Fire");
        fireNode->transform.position = glm::vec3(0.0f, 0.3f, 0.0f);
        auto* fire = fireNode->AddComponent<ParticleSystemComponent>();
        fire->ApplyPreset(ParticleSystemComponent::Preset::Fire);
        vfxScene->AddNodeToRoot(fireNode);
        vfxScene->RegisterAnimator(fireNode);

        TNode* smokeNode = new TNode(nullptr, "Campfire Smoke");
        smokeNode->transform.position = glm::vec3(0.0f, 1.2f, 0.0f);
        auto* smoke = smokeNode->AddComponent<ParticleSystemComponent>();
        smoke->ApplyPreset(ParticleSystemComponent::Preset::Smoke);
        vfxScene->AddNodeToRoot(smokeNode);
        vfxScene->RegisterAnimator(smokeNode);

        // A sparks fountain off to the side.
        TNode* sparksNode = new TNode(nullptr, "Sparks");
        sparksNode->transform.position = glm::vec3(6.0f, 0.5f, 3.0f);
        auto* sparks = sparksNode->AddComponent<ParticleSystemComponent>();
        sparks->ApplyPreset(ParticleSystemComponent::Preset::Sparks);
        vfxScene->AddNodeToRoot(sparksNode);
        vfxScene->RegisterAnimator(sparksNode);

        // A lone billboard (untextured tinted quad; drop a texture in the
        // Inspector to make it a sprite/impostor).
        TNode* billboardNode = new TNode(nullptr, "Billboard");
        billboardNode->transform.position = glm::vec3(-6.0f, 2.0f, 2.0f);
        auto* billboard = billboardNode->AddComponent<BillboardComponent>();
        billboard->size = glm::vec2(2.0f, 3.0f);
        billboard->tint = glm::vec4(0.4f, 0.8f, 1.0f, 0.9f);
        vfxScene->AddNodeToRoot(billboardNode);

        // Skybox + a sun so the grass reads well.
        auto skyboxCubemap = ResourceManager::LoadSkyboxFromFolder("space");
        if (skyboxCubemap) {
            vfxScene->SetSkybox(std::make_shared<Skybox>(skyboxCubemap, "space"));
        }

        TNode* sunNode = new TNode(nullptr, "Sun");
        auto* sun = sunNode->AddComponent<LightComponent>(sunNode, LightType::Directional);
        sun->intensity = 3.0f;
        sunNode->transform.rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
        vfxScene->AddNodeToRoot(sunNode);

        TNode* vfxCamera = new TNode(nullptr, "MainCamera");
        CameraComponent* cam = vfxCamera->AddComponent<CameraComponent>(vfxCamera);
        vfxCamera->transform.position = glm::vec3(0.0f, 5.0f, 14.0f);
        cam->pitch = -18.0f;
        vfxScene->AddNodeToRoot(vfxCamera);
        vfxScene->SetMainCamera(vfxCamera);
    }

    if (!sm.GetScene("Terrain Test")) {
        Scene* terrainScene = sm.CreateScene("Terrain Test");

        // Procedural terrain (no heightmap asset needed -- falls back to rolling
        // hills). Drag a grayscale image onto its Heightmap slot in the Inspector
        // to use a real one.
        TNode* terrainNode = new TNode(nullptr, "Terrain");
        auto* terrain = terrainNode->AddComponent<TerrainComponent>(terrainNode);
        terrain->size = 120.0f;
        terrain->resolution = 200;
        terrain->heightScale = 14.0f;
        terrain->noiseFrequency = 0.08f;
        terrain->Generate();
        terrainScene->AddNodeToRoot(terrainNode);

        // Fog to show off distance atmosphere over the terrain.
        FogSettings& fog = terrainScene->GetFog();
        fog.enabled = true;
        fog.mode = FogMode::Exp2;
        fog.color = glm::vec3(0.62f, 0.67f, 0.75f);
        fog.density = 0.012f;
        terrainScene->SetClearColor(fog.color);

        TNode* sunNode = new TNode(nullptr, "Sun");
        auto* sun = sunNode->AddComponent<LightComponent>(sunNode, LightType::Directional);
        sun->intensity = 3.0f;
        sunNode->transform.rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
        terrainScene->AddNodeToRoot(sunNode);

        TNode* terrainCamera = new TNode(nullptr, "MainCamera");
        CameraComponent* cam = terrainCamera->AddComponent<CameraComponent>(terrainCamera);
        terrainCamera->transform.position = glm::vec3(0.0f, 25.0f, 55.0f);
        cam->pitch = -22.0f;
        cam->farPlane = 500.0f;
        terrainScene->AddNodeToRoot(terrainCamera);
        terrainScene->SetMainCamera(terrainCamera);
    }

    if (!sm.GetScene("Cartoon Test")) {
        Scene* cartoonScene = sm.CreateScene("Cartoon Test");
        cartoonScene->SetClearColor(glm::vec3(0.55f, 0.75f, 0.9f));

        cartoonScene->AddNodeToRoot(BuildShadedGround("cartoon", 60.0f, glm::vec4(0.5f, 0.8f, 0.5f, 1.0f)));
        AddPrimitiveShowcase(cartoonScene, "cartoon");

        // Key light: a warm sun giving the main toon bands their direction.
        TNode* sunNode = new TNode(nullptr, "Sun");
        auto* sun = sunNode->AddComponent<LightComponent>(sunNode, LightType::Directional);
        sun->intensity = 2.2f;
        sun->color = glm::vec3(1.0f, 0.95f, 0.85f);
        sunNode->transform.rotation = glm::vec3(-45.0f, -35.0f, 0.0f);
        cartoonScene->AddNodeToRoot(sunNode);

        // Colored point lights from different sides -- with cel-shading these
        // paint clearly separated colored bands across the surfaces.
        AddPointLight(cartoonScene, "Red Light",     glm::vec3(-7.0f, 3.0f,  4.0f), glm::vec3(1.0f, 0.25f, 0.2f),  4.0f, 22.0f);
        AddPointLight(cartoonScene, "Cyan Light",    glm::vec3( 7.0f, 3.0f,  4.0f), glm::vec3(0.2f, 0.7f, 1.0f),   4.0f, 22.0f);
        AddPointLight(cartoonScene, "Magenta Light", glm::vec3( 0.0f, 5.0f, -8.0f), glm::vec3(0.9f, 0.3f, 0.9f),   4.0f, 26.0f);

        TNode* cartoonCamera = new TNode(nullptr, "MainCamera");
        CameraComponent* cam = cartoonCamera->AddComponent<CameraComponent>(cartoonCamera);
        cartoonCamera->transform.position = glm::vec3(0.0f, 4.0f, 12.0f);
        cam->pitch = -16.0f;
        cartoonScene->AddNodeToRoot(cartoonCamera);
        cartoonScene->SetMainCamera(cartoonCamera);
    }

    if (!sm.GetScene("Retro Test")) {
        Scene* retroScene = sm.CreateScene("Retro Test");
        // Obra-Dinn-style 1-bit dithering: the shader outputs only black/white,
        // so the clear color barely matters, but keep it dark for framing.
        retroScene->SetClearColor(glm::vec3(0.05f, 0.05f, 0.06f));
        retroScene->SetGridVisible(false);

        retroScene->AddNodeToRoot(BuildShadedGround("retro", 60.0f, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)));
        AddPrimitiveShowcase(retroScene, "retro");

        // The retro shader keys off luminance (not light color), so what matters
        // here is having light coming from several directions: each creates its
        // own patch of white dots on a surface, and the dark shader keeps
        // everything else in ink. A soft key + point lights near the objects
        // carve out readable lit regions against the black.
        TNode* sunNode = new TNode(nullptr, "Sun");
        auto* sun = sunNode->AddComponent<LightComponent>(sunNode, LightType::Directional);
        sun->intensity = 1.6f;
        sunNode->transform.rotation = glm::vec3(-45.0f, -35.0f, 0.0f);
        retroScene->AddNodeToRoot(sunNode);

        // Point lights hugging the objects -- these are what make the shapes
        // "emerge" from the darkness as bright dithered highlights.
        AddPointLight(retroScene, "Key Point",   glm::vec3(-4.0f, 4.0f,  5.0f), glm::vec3(1.0f), 6.0f, 24.0f);
        AddPointLight(retroScene, "Side Point",  glm::vec3( 6.0f, 3.0f,  1.0f), glm::vec3(1.0f), 5.0f, 20.0f);
        AddPointLight(retroScene, "Back Point",  glm::vec3( 0.0f, 5.0f, -7.0f), glm::vec3(1.0f), 6.0f, 26.0f);

        TNode* retroCamera = new TNode(nullptr, "MainCamera");
        CameraComponent* cam = retroCamera->AddComponent<CameraComponent>(retroCamera);
        retroCamera->transform.position = glm::vec3(0.0f, 4.0f, 12.0f);
        cam->pitch = -16.0f;
        retroScene->AddNodeToRoot(retroCamera);
        retroScene->SetMainCamera(retroCamera);
    }

    if (!sm.GetScene("Water Test")) {
        Scene* waterScene = sm.CreateScene("Water Test");
        waterScene->SetClearColor(glm::vec3(0.1f, 0.15f, 0.2f));

        // A big subdivided plane IS the water surface -- the shader ripples its
        // normal procedurally over time (the mesh stays flat). Transparent so it
        // reads as liquid. Blue tint = water; change baseColor for toxic/lava.
        std::vector<MeshVertex> v;
        std::vector<uint32_t> idx;
        MeshPrimitives::Plane(v, idx, 40.0f, 1);
        TNode* water = BuildShadedNode("Water Surface", "water", v, idx, glm::vec4(0.1f, 0.5f, 0.8f, 0.85f), /*transparent=*/true);
        water->transform.position = glm::vec3(0.0f, 0.5f, 0.0f);
        waterScene->AddNodeToRoot(water);

        // A few opaque (pbr) objects half-submerged, so the water has something
        // to sit around and reflect light near.
        v.clear(); idx.clear();
        MeshPrimitives::Sphere(v, idx, 2.0f, 32, 24);
        TNode* rock = BuildShadedNode("Rock", "pbr", v, idx, glm::vec4(0.35f, 0.3f, 0.28f, 1.0f));
        rock->transform.position = glm::vec3(-4.0f, 0.5f, -2.0f);
        waterScene->AddNodeToRoot(rock);

        TNode* sunNode = new TNode(nullptr, "Sun");
        auto* sun = sunNode->AddComponent<LightComponent>(sunNode, LightType::Directional);
        sun->intensity = 2.5f;
        sun->color = glm::vec3(1.0f, 0.97f, 0.9f);
        sunNode->transform.rotation = glm::vec3(-55.0f, -25.0f, 0.0f);
        waterScene->AddNodeToRoot(sunNode);
        AddPointLight(waterScene, "Glow", glm::vec3(3.0f, 3.0f, 3.0f), glm::vec3(0.4f, 0.8f, 1.0f), 4.0f, 20.0f);

        TNode* waterCamera = new TNode(nullptr, "MainCamera");
        CameraComponent* cam = waterCamera->AddComponent<CameraComponent>(waterCamera);
        waterCamera->transform.position = glm::vec3(0.0f, 6.0f, 16.0f);
        cam->pitch = -22.0f;
        waterScene->AddNodeToRoot(waterCamera);
        waterScene->SetMainCamera(waterCamera);
    }

    if (!sm.GetScene("Hologram Test")) {
        Scene* holoScene = sm.CreateScene("Hologram Test");
        holoScene->SetClearColor(glm::vec3(0.03f, 0.04f, 0.06f)); // dark so the glow pops
        holoScene->SetGridVisible(false);

        // Transparent hologram primitives in a row (cyan projection look).
        std::vector<MeshVertex> v;
        std::vector<uint32_t> idx;
        glm::vec4 cyan(0.3f, 0.9f, 1.0f, 1.0f);

        MeshPrimitives::Sphere(v, idx, 1.4f, 32, 24);
        TNode* holoSphere = BuildShadedNode("Holo Sphere", "hologram", v, idx, cyan, /*transparent=*/true);
        holoSphere->transform.position = glm::vec3(-3.5f, 1.6f, 0.0f);
        holoScene->AddNodeToRoot(holoSphere);

        v.clear(); idx.clear();
        MeshPrimitives::Cone(v, idx, 1.2f, 2.8f, 32);
        TNode* holoCone = BuildShadedNode("Holo Cone", "hologram", v, idx, cyan, /*transparent=*/true);
        holoCone->transform.position = glm::vec3(0.0f, 1.6f, 0.0f);
        holoScene->AddNodeToRoot(holoCone);

        v.clear(); idx.clear();
        MeshPrimitives::Cylinder(v, idx, 1.0f, 2.8f, 32);
        TNode* holoCyl = BuildShadedNode("Holo Cylinder", "hologram", v, idx, cyan, /*transparent=*/true);
        holoCyl->transform.position = glm::vec3(3.5f, 1.6f, 0.0f);
        holoScene->AddNodeToRoot(holoCyl);

        TNode* holoCamera = new TNode(nullptr, "MainCamera");
        CameraComponent* cam = holoCamera->AddComponent<CameraComponent>(holoCamera);
        holoCamera->transform.position = glm::vec3(0.0f, 3.0f, 9.0f);
        cam->pitch = -12.0f;
        holoScene->AddNodeToRoot(holoCamera);
        holoScene->SetMainCamera(holoCamera);
    }

    if (!sm.GetScene("Iridescent Test")) {
        Scene* iriScene = sm.CreateScene("Iridescent Test");
        iriScene->SetClearColor(glm::vec3(0.06f, 0.06f, 0.08f));

        iriScene->AddNodeToRoot(BuildShadedGround("pbr", 40.0f, glm::vec4(0.15f, 0.15f, 0.18f, 1.0f)));
        // Iridescent gems: the hue shifts with view angle, so orbit the camera
        // or rotate them to see the rainbow move.
        AddPrimitiveShowcase(iriScene, "iridescent");

        TNode* sunNode = new TNode(nullptr, "Sun");
        auto* sun = sunNode->AddComponent<LightComponent>(sunNode, LightType::Directional);
        sun->intensity = 2.0f;
        sunNode->transform.rotation = glm::vec3(-45.0f, -35.0f, 0.0f);
        iriScene->AddNodeToRoot(sunNode);
        AddPointLight(iriScene, "Sparkle", glm::vec3(0.0f, 5.0f, 5.0f), glm::vec3(1.0f), 4.0f, 24.0f);

        TNode* iriCamera = new TNode(nullptr, "MainCamera");
        CameraComponent* cam = iriCamera->AddComponent<CameraComponent>(iriCamera);
        iriCamera->transform.position = glm::vec3(0.0f, 4.0f, 12.0f);
        cam->pitch = -16.0f;
        iriScene->AddNodeToRoot(iriCamera);
        iriScene->SetMainCamera(iriCamera);
    }

    if (!sm.GetScene("Texture FX Test")) {
        Scene* fxScene = sm.CreateScene("Texture FX Test");
        fxScene->SetClearColor(glm::vec3(0.08f, 0.08f, 0.1f));

        // Left: scrolling UVs (conveyor/waterfall/lava look). Right: heat-haze
        // distortion. Both fall back to a procedural checker when no albedo
        // texture is assigned, so the animation is visible without an asset --
        // drag a texture onto the material's Albedo slot to use a real image.
        std::vector<MeshVertex> v;
        std::vector<uint32_t> idx;

        // A large upright quad (two triangles) for each effect, facing +Z.
        auto makeQuad = [](std::vector<MeshVertex>& outV, std::vector<uint32_t>& outI, float halfW, float halfH) {
            outV = {
                { {-halfW, -halfH, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
                { { halfW, -halfH, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
                { { halfW,  halfH, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },
                { {-halfW,  halfH, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
            };
            outI = { 0, 1, 2, 0, 2, 3 };
        };

        makeQuad(v, idx, 3.0f, 3.0f);
        TNode* scrollPanel = BuildShadedNode("Scrolling Panel", "scrolling", v, idx, glm::vec4(1.0f, 0.7f, 0.3f, 1.0f));
        scrollPanel->transform.position = glm::vec3(-3.5f, 3.0f, 0.0f);
        fxScene->AddNodeToRoot(scrollPanel);

        v.clear(); idx.clear();
        makeQuad(v, idx, 3.0f, 3.0f);
        TNode* distortPanel = BuildShadedNode("Distortion Panel", "distortion", v, idx, glm::vec4(0.6f, 0.9f, 1.0f, 1.0f));
        distortPanel->transform.position = glm::vec3(3.5f, 3.0f, 0.0f);
        fxScene->AddNodeToRoot(distortPanel);

        fxScene->AddNodeToRoot(BuildShadedGround("pbr", 40.0f, glm::vec4(0.3f, 0.3f, 0.33f, 1.0f)));

        TNode* sunNode = new TNode(nullptr, "Sun");
        auto* sun = sunNode->AddComponent<LightComponent>(sunNode, LightType::Directional);
        sun->intensity = 2.5f;
        sunNode->transform.rotation = glm::vec3(-40.0f, -20.0f, 0.0f);
        fxScene->AddNodeToRoot(sunNode);

        TNode* fxCamera = new TNode(nullptr, "MainCamera");
        CameraComponent* cam = fxCamera->AddComponent<CameraComponent>(fxCamera);
        fxCamera->transform.position = glm::vec3(0.0f, 3.5f, 12.0f);
        cam->pitch = -6.0f;
        fxScene->AddNodeToRoot(fxCamera);
        fxScene->SetMainCamera(fxCamera);
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
