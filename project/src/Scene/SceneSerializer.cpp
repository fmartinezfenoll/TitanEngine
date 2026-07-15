#include "Scene/SceneSerializer.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/AnimationComponent.h"
#include "Scene/AnimationClip.h"
#include "Scene/AnimationStateMachine.h"
#include "Scene/CameraPathComponent.h"
#include "Scene/SkinComponent.h"
#include "Scene/PatrolComponent.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include "ResourceManager/CubemapTexture.h"
#include "Renderer/Skybox.h"
#include "Core/Log.h"
#include <json.hpp>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <functional>

using json = nlohmann::json;

namespace {

json SerializeTransform(const Transform& transform) {
    json j;
    j["position"] = {transform.position.x, transform.position.y, transform.position.z};
    j["rotation"] = {transform.rotation.x, transform.rotation.y, transform.rotation.z};
    j["scale"] = {transform.scale.x, transform.scale.y, transform.scale.z};
    return j;
}

Transform DeserializeTransform(const json& j) {
    Transform t;
    if (j.contains("position") && j["position"].is_array()) {
        auto pos = j["position"];
        t.position = {pos[0], pos[1], pos[2]};
    }
    if (j.contains("rotation") && j["rotation"].is_array()) {
        auto rot = j["rotation"];
        t.rotation = {rot[0], rot[1], rot[2]};
    }
    if (j.contains("scale") && j["scale"].is_array()) {
        auto scl = j["scale"];
        t.scale = {scl[0], scl[1], scl[2]};
    }
    return t;
}

json SerializeBoundingVolume(const BoundingVolume* boundingBox) {
    if (!boundingBox) {
        return json::object();
    }

    // Try to cast to known types
    if (const Sphere* sphere = dynamic_cast<const Sphere*>(boundingBox)) {
        json j;
        j["type"] = "sphere";
        j["center"] = {sphere->center.x, sphere->center.y, sphere->center.z};
        j["radius"] = sphere->radius;
        return j;
    }

    if (const AABB* aabb = dynamic_cast<const AABB*>(boundingBox)) {
        json j;
        j["type"] = "aabb";
        j["center"] = {aabb->center.x, aabb->center.y, aabb->center.z};
        j["extents"] = {aabb->extents.x, aabb->extents.y, aabb->extents.z};
        return j;
    }

    return json::object();
}

BoundingVolume* DeserializeBoundingVolume(const json& j) {
    if (j.is_null() || j.empty()) {
        return nullptr;
    }

    if (!j.contains("type")) {
        return nullptr;
    }

    std::string type = j["type"];

    if (type == "sphere" && j.contains("center") && j.contains("radius")) {
        auto center_arr = j["center"];
        glm::vec3 center = {center_arr[0], center_arr[1], center_arr[2]};
        float radius = j["radius"];
        return new Sphere(center, radius);
    }

    if (type == "aabb" && j.contains("center") && j.contains("extents")) {
        auto center_arr = j["center"];
        auto extents_arr = j["extents"];
        glm::vec3 center = {center_arr[0], center_arr[1], center_arr[2]};
        glm::vec3 extents = {extents_arr[0], extents_arr[1], extents_arr[2]};
        glm::vec3 min = center - extents;
        glm::vec3 max = center + extents;
        return new AABB(min, max);
    }

    return nullptr;
}

json SerializeComponents(const TNode* node) {
    json arr = json::array();

    if (auto* mesh = node->GetComponent<MeshComponent>()) {
        json j;
        j["type"] = "mesh";

        json vertices = json::array();
        for (const MeshVertex& v : mesh->GetVertices()) {
            vertices.push_back(v.position.x);
            vertices.push_back(v.position.y);
            vertices.push_back(v.position.z);
            vertices.push_back(v.normal.x);
            vertices.push_back(v.normal.y);
            vertices.push_back(v.normal.z);
            vertices.push_back(v.uv.x);
            vertices.push_back(v.uv.y);
        }
        j["vertices"] = vertices;
        j["indices"] = mesh->GetIndices();

        if (mesh->HasSkinning()) {
            json joints = json::array();
            json weights = json::array();
            for (const MeshVertex& v : mesh->GetVertices()) {
                joints.push_back({v.jointIndices.x, v.jointIndices.y, v.jointIndices.z, v.jointIndices.w});
                weights.push_back({v.jointWeights.x, v.jointWeights.y, v.jointWeights.z, v.jointWeights.w});
            }
            j["joints"] = joints;
            j["weights"] = weights;
        }

        arr.push_back(j);
    }

    if (auto* materialComp = node->GetComponent<MaterialComponent>()) {
        if (const auto& mat = materialComp->material) {
            json j;
            j["type"] = "material";

            auto shader = mat->GetShader();
            j["shader"] = shader ? shader->GetName() : "";
            j["baseColor"] = {mat->baseColor.r, mat->baseColor.g, mat->baseColor.b, mat->baseColor.a};
            j["metallicFactor"] = mat->metallicFactor;
            j["roughnessFactor"] = mat->roughnessFactor;
            j["transparent"] = mat->transparent;

            auto serializeTextureSlot = [&](const char* key, const std::shared_ptr<Texture>& tex) {
                if (!tex) return;

                if (tex->GetFilePath().empty()) {
                    // Embedded glTF texture (no source file on disk) -- bake it out to a
                    // real PNG once so it can be persisted and reloaded like any other asset.
                    std::filesystem::create_directories("resources/textures/generated");
                    std::string safeName = tex->GetName();
                    for (char& c : safeName) {
                        if (c == '/' || c == '\\' || c == ':' || c == '.') c = '_';
                    }
                    std::string bakedPath = "resources/textures/generated/" + safeName + ".png";
                    if (!tex->SaveToPNG(bakedPath)) {
                        Log::Info("SceneSerializer: skipping embedded texture on material for node '" + node->name +
                                  "' (failed to bake to disk)");
                        return;
                    }
                }

                json t;
                t["name"] = tex->GetName();
                t["path"] = tex->GetFilePath();
                j[key] = t;
            };
            serializeTextureSlot("albedo", mat->albedo);
            serializeTextureSlot("normal", mat->normal);
            serializeTextureSlot("metallicRoughness", mat->metallicRoughness);

            arr.push_back(j);
        }
    }

    if (auto* camera = node->GetComponent<CameraComponent>()) {
        json j;
        j["type"] = "camera";
        j["fov"] = camera->fov;
        j["nearPlane"] = camera->nearPlane;
        j["farPlane"] = camera->farPlane;
        j["moveSpeed"] = camera->moveSpeed;
        j["mouseSensitivity"] = camera->mouseSensitivity;
        j["yaw"] = camera->yaw;
        j["pitch"] = camera->pitch;

        arr.push_back(j);
    }

    if (auto* light = node->GetComponent<LightComponent>()) {
        json j;
        j["type"] = "light";
        j["lightType"] = static_cast<int>(light->type);
        j["color"] = {light->color.r, light->color.g, light->color.b};
        j["intensity"] = light->intensity;
        j["range"] = light->range;
        j["innerConeDegrees"] = light->innerConeDegrees;
        j["outerConeDegrees"] = light->outerConeDegrees;

        arr.push_back(j);
    }

    if (auto* anim = node->GetComponent<AnimationComponent>()) {
        json j;
        j["type"] = "animation";
        j["playOnStart"] = anim->GetPlayOnStart();
        j["playOnStartClip"] = anim->GetPlayOnStartClip();

        json clipsJson = json::array();
        for (const AnimationClip& clip : anim->GetClips()) {
            json clipJson;
            clipJson["name"] = clip.name;
            clipJson["duration"] = clip.duration;

            json channelsJson = json::array();
            for (const AnimationChannelData& channel : clip.channels) {
                json cj;
                cj["targetNodeIndex"] = channel.targetNodeIndex;
                cj["path"] = static_cast<int>(channel.path);
                cj["interpolation"] = static_cast<int>(channel.interpolation);

                if (channel.path == AnimationTargetPath::Rotation) {
                    json keys = json::array();
                    for (const auto& k : channel.quatKeys) {
                        keys.push_back({k.time, k.value.w, k.value.x, k.value.y, k.value.z});
                    }
                    cj["quatKeys"] = keys;
                } else {
                    json keys = json::array();
                    for (const auto& k : channel.vec3Keys) {
                        keys.push_back({k.time, k.value.x, k.value.y, k.value.z});
                    }
                    cj["vec3Keys"] = keys;
                }

                channelsJson.push_back(cj);
            }
            clipJson["channels"] = channelsJson;
            clipsJson.push_back(clipJson);
        }
        j["clips"] = clipsJson;

        if (const AnimationStateMachine* machine = anim->GetStateMachine()) {
            json smJson;
            smJson["initialState"] = machine->GetInitialState();

            json statesJson = json::array();
            for (const auto& state : machine->GetStates()) {
                statesJson.push_back({{"name", state.name}, {"clip", state.clipName}, {"loop", state.loop}});
            }
            smJson["states"] = statesJson;

            json transitionsJson = json::array();
            for (const auto& transition : machine->GetTransitions()) {
                transitionsJson.push_back({
                    {"fromState", transition.fromState},
                    {"toState", transition.toState},
                    {"parameter", transition.parameter},
                    {"op", static_cast<int>(transition.op)},
                    {"threshold", transition.threshold},
                    {"blendSeconds", transition.blendSeconds}
                });
            }
            smJson["transitions"] = transitionsJson;

            j["stateMachine"] = smJson;
        }

        arr.push_back(j);
    }

    if (auto* skin = node->GetComponent<SkinComponent>()) {
        json j;
        j["type"] = "skin";

        // Joints are stored as indices into a pre-order walk of the MESH NODE'S
        // OWN root ancestor -- specifically, the nearest ancestor (or itself)
        // that owns an AnimationComponent, which is always the model's root and
        // matches the same pre-order numbering GLTFLoader::ProcessAnimations
        // already uses. Resolved by a post-process pass in LoadScene/DuplicateNode/
        // DeserializeNodeFromString once the whole subtree exists.
        const TNode* animRoot = node;
        while (animRoot->parent && !animRoot->GetComponent<AnimationComponent>()) {
            animRoot = animRoot->parent;
        }

        std::unordered_map<const TNode*, int> preOrder;
        int counter = 0;
        std::function<void(const TNode*)> walk = [&](const TNode* n) {
            preOrder[n] = counter++;
            for (const TNode* child : n->children) walk(child);
        };
        walk(animRoot);

        json jointsJson = json::array();
        const auto& joints = skin->GetJoints();
        const auto& invBind = skin->GetInverseBindMatrices();
        for (size_t i = 0; i < joints.size(); ++i) {
            json jj;
            auto it = joints[i] ? preOrder.find(joints[i]) : preOrder.end();
            jj["nodeIndex"] = (it != preOrder.end()) ? it->second : -1;

            json m = json::array();
            const glm::mat4& mat = invBind[i];
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    m.push_back(mat[c][r]);
            jj["inverseBindMatrix"] = m;

            jointsJson.push_back(jj);
        }
        j["joints"] = jointsJson;

        arr.push_back(j);
    }

    if (auto* patrol = node->GetComponent<PatrolComponent>()) {
        json j;
        j["type"] = "patrol";
        j["speed"] = patrol->GetSpeed();
        j["turnSpeed"] = patrol->GetTurnSpeed();
        j["forwardOffset"] = patrol->GetForwardOffset();
        j["active"] = patrol->IsActive();

        json waypointsJson = json::array();
        for (const PatrolWaypoint& wp : patrol->GetWaypoints()) {
            json wj;
            wj["position"] = {wp.position.x, wp.position.y, wp.position.z};
            wj["pause"] = wp.pauseSeconds;
            waypointsJson.push_back(wj);
        }
        j["waypoints"] = waypointsJson;

        arr.push_back(j);
    }

    if (auto* cameraPath = node->GetComponent<CameraPathComponent>()) {
        json j;
        j["type"] = "cameraPath";
        j["loop"] = cameraPath->IsLooping();

        json pointsJson = json::array();
        for (const CameraPathPoint& point : cameraPath->GetPoints()) {
            json pj;
            pj["position"] = {point.position.x, point.position.y, point.position.z};
            pj["lookAt"] = {point.lookAt.x, point.lookAt.y, point.lookAt.z};
            pj["travelSeconds"] = point.travelSeconds;
            pj["holdSeconds"] = point.holdSeconds;
            pointsJson.push_back(pj);
        }
        j["points"] = pointsJson;

        arr.push_back(j);
    }

    return arr;
}

// Nodes with a freshly-deserialized AnimationComponent, and pending SkinComponent
// joint data (pre-order indices not yet resolved to TNode*) -- both are resolved
// in one pass, per animated-subtree root, by ResolvePendingAnimationData() once
// the whole node tree exists (see DeserializeNode's post-order return point).
thread_local std::vector<TNode*> g_pendingAnimationRoots;

struct PendingSkin {
    TNode* meshNode;
    std::vector<int> jointPreOrderIndices;
    std::vector<glm::mat4> inverseBindMatrices;
};
thread_local std::vector<PendingSkin> g_pendingSkins;

void DeserializeComponents(TNode* node, Scene* scene, const json& j) {
    if (!j.is_array()) return;

    for (const auto& compJson : j) {
        if (!compJson.contains("type")) continue;
        std::string type = compJson["type"];

        if (type == "mesh" && compJson.contains("vertices") && compJson.contains("indices")) {
            std::vector<float> flat = compJson["vertices"].get<std::vector<float>>();
            std::vector<MeshVertex> vertices;
            vertices.reserve(flat.size() / 8);
            for (size_t i = 0; i + 7 < flat.size(); i += 8) {
                MeshVertex v;
                v.position = {flat[i], flat[i + 1], flat[i + 2]};
                v.normal = {flat[i + 3], flat[i + 4], flat[i + 5]};
                v.uv = {flat[i + 6], flat[i + 7]};
                vertices.push_back(v);
            }
            std::vector<uint32_t> indices = compJson["indices"].get<std::vector<uint32_t>>();

            if (compJson.contains("joints") && compJson.contains("weights")) {
                const auto& jointsArr = compJson["joints"];
                const auto& weightsArr = compJson["weights"];
                for (size_t i = 0; i < vertices.size() && i < jointsArr.size() && i < weightsArr.size(); ++i) {
                    const auto& ja = jointsArr[i];
                    const auto& wa = weightsArr[i];
                    vertices[i].jointIndices = glm::ivec4(ja[0], ja[1], ja[2], ja[3]);
                    vertices[i].jointWeights = glm::vec4(wa[0], wa[1], wa[2], wa[3]);
                }
            }

            node->AddComponent<MeshComponent>(vertices, indices);
        }
        else if (type == "material" && compJson.contains("shader")) {
            std::string shaderName = compJson["shader"];
            auto shader = ResourceManager::LoadShader(shaderName);
            auto material = std::make_shared<Material>(shader);

            if (compJson.contains("baseColor") && compJson["baseColor"].is_array()) {
                auto c = compJson["baseColor"];
                material->baseColor = {c[0], c[1], c[2], c[3]};
            }
            if (compJson.contains("metallicFactor")) material->metallicFactor = compJson["metallicFactor"];
            if (compJson.contains("roughnessFactor")) material->roughnessFactor = compJson["roughnessFactor"];
            if (compJson.contains("transparent")) material->transparent = compJson["transparent"];

            auto deserializeTextureSlot = [&](const char* key, std::shared_ptr<Texture>& slot) {
                if (!compJson.contains(key)) return;
                const auto& t = compJson[key];
                if (!t.contains("name") || !t.contains("path")) return;
                slot = ResourceManager::LoadTexture(t["name"], t["path"]);
            };
            deserializeTextureSlot("albedo", material->albedo);
            deserializeTextureSlot("normal", material->normal);
            deserializeTextureSlot("metallicRoughness", material->metallicRoughness);

            node->AddComponent<MaterialComponent>(material);
        }
        else if (type == "camera") {
            auto* camera = node->AddComponent<CameraComponent>(node);
            if (compJson.contains("fov")) camera->fov = compJson["fov"];
            if (compJson.contains("nearPlane")) camera->nearPlane = compJson["nearPlane"];
            if (compJson.contains("farPlane")) camera->farPlane = compJson["farPlane"];
            if (compJson.contains("moveSpeed")) camera->moveSpeed = compJson["moveSpeed"];
            if (compJson.contains("mouseSensitivity")) camera->mouseSensitivity = compJson["mouseSensitivity"];
            if (compJson.contains("yaw")) camera->yaw = compJson["yaw"];
            if (compJson.contains("pitch")) camera->pitch = compJson["pitch"];

            if (scene) {
                scene->RegisterCamera(node);
            }
        }
        else if (type == "light") {
            LightType lightType = LightType::Point;
            if (compJson.contains("lightType")) {
                lightType = static_cast<LightType>(compJson["lightType"].get<int>());
            }

            auto* light = node->AddComponent<LightComponent>(node, lightType);
            if (compJson.contains("color") && compJson["color"].is_array()) {
                auto c = compJson["color"];
                light->color = {c[0], c[1], c[2]};
            }
            if (compJson.contains("intensity")) light->intensity = compJson["intensity"];
            if (compJson.contains("range")) light->range = compJson["range"];
            if (compJson.contains("innerConeDegrees")) light->innerConeDegrees = compJson["innerConeDegrees"];
            if (compJson.contains("outerConeDegrees")) light->outerConeDegrees = compJson["outerConeDegrees"];

            if (scene) {
                scene->RegisterLight(node);
            }
        }
        else if (type == "animation" && compJson.contains("clips")) {
            auto* anim = node->AddComponent<AnimationComponent>(node);

            for (const auto& clipJson : compJson["clips"]) {
                AnimationClip clip;
                clip.name = clipJson.value("name", "");
                clip.duration = clipJson.value("duration", 0.0f);

                if (clipJson.contains("channels")) {
                    for (const auto& cj : clipJson["channels"]) {
                        AnimationChannelData channel;
                        channel.targetNodeIndex = cj.value("targetNodeIndex", -1);
                        channel.path = static_cast<AnimationTargetPath>(cj.value("path", 0));
                        channel.interpolation = static_cast<AnimationInterpolation>(cj.value("interpolation", 0));

                        if (cj.contains("quatKeys")) {
                            for (const auto& k : cj["quatKeys"]) {
                                AnimationKeyframeQuat kf;
                                kf.time = k[0];
                                kf.value = glm::quat(k[1], k[2], k[3], k[4]);
                                channel.quatKeys.push_back(kf);
                            }
                        }
                        if (cj.contains("vec3Keys")) {
                            for (const auto& k : cj["vec3Keys"]) {
                                AnimationKeyframeVec3 kf;
                                kf.time = k[0];
                                kf.value = glm::vec3(k[1], k[2], k[3]);
                                channel.vec3Keys.push_back(kf);
                            }
                        }

                        clip.channels.push_back(std::move(channel));
                    }
                }

                anim->AddClip(std::move(clip));
            }

            if (compJson.contains("playOnStart")) {
                anim->SetPlayOnStart(compJson.value("playOnStart", false), compJson.value("playOnStartClip", ""));
            }

            if (compJson.contains("stateMachine")) {
                const auto& smJson = compJson["stateMachine"];
                AnimationStateMachine* machine = anim->GetOrCreateStateMachine();

                if (smJson.contains("states")) {
                    for (const auto& sj : smJson["states"]) {
                        machine->AddState(sj.value("name", ""), sj.value("clip", ""), sj.value("loop", true));
                    }
                }
                if (smJson.contains("transitions")) {
                    for (const auto& tj : smJson["transitions"]) {
                        machine->AddTransition(
                            tj.value("fromState", ""),
                            tj.value("toState", ""),
                            tj.value("parameter", ""),
                            static_cast<AnimationStateMachine::ConditionOp>(tj.value("op", 2)),
                            tj.value("threshold", 0.0f),
                            tj.value("blendSeconds", 0.2f));
                    }
                }
                if (smJson.contains("initialState")) {
                    machine->SetInitialState(smJson["initialState"]);
                }
            }

            if (scene) {
                scene->RegisterAnimator(node);
            }
            g_pendingAnimationRoots.push_back(node);
        }
        else if (type == "skin" && compJson.contains("joints")) {
            std::vector<int> jointPreOrderIndices;
            std::vector<glm::mat4> inverseBindMatrices;

            for (const auto& jj : compJson["joints"]) {
                jointPreOrderIndices.push_back(jj.value("nodeIndex", -1));

                glm::mat4 m(1.0f);
                if (jj.contains("inverseBindMatrix")) {
                    const auto& arr = jj["inverseBindMatrix"];
                    for (int c = 0; c < 4; ++c)
                        for (int r = 0; r < 4; ++r)
                            m[c][r] = arr[c * 4 + r];
                }
                inverseBindMatrices.push_back(m);
            }

            g_pendingSkins.push_back({node, std::move(jointPreOrderIndices), std::move(inverseBindMatrices)});
        }
        else if (type == "patrol") {
            auto* patrol = node->AddComponent<PatrolComponent>(node);
            if (compJson.contains("speed")) patrol->SetSpeed(compJson["speed"]);
            if (compJson.contains("turnSpeed")) patrol->SetTurnSpeed(compJson["turnSpeed"]);
            if (compJson.contains("forwardOffset")) patrol->SetForwardOffset(compJson["forwardOffset"]);
            if (compJson.contains("active")) patrol->SetActive(compJson["active"]);

            if (compJson.contains("waypoints")) {
                for (const auto& wj : compJson["waypoints"]) {
                    glm::vec3 pos(0.0f);
                    if (wj.contains("position") && wj["position"].is_array()) {
                        auto p = wj["position"];
                        pos = glm::vec3(p[0], p[1], p[2]);
                    }
                    float pause = wj.value("pause", 0.0f);
                    patrol->AddWaypoint(pos, pause);
                }
            }

            if (scene) {
                scene->RegisterAnimator(node);
            }
        }
        else if (type == "cameraPath") {
            auto* cameraPath = node->AddComponent<CameraPathComponent>(node);
            if (compJson.contains("loop")) cameraPath->SetLooping(compJson["loop"]);

            if (compJson.contains("points")) {
                for (const auto& pj : compJson["points"]) {
                    glm::vec3 pos(0.0f), lookAt(0.0f);
                    if (pj.contains("position") && pj["position"].is_array()) {
                        auto p = pj["position"];
                        pos = glm::vec3(p[0], p[1], p[2]);
                    }
                    if (pj.contains("lookAt") && pj["lookAt"].is_array()) {
                        auto l = pj["lookAt"];
                        lookAt = glm::vec3(l[0], l[1], l[2]);
                    }
                    float travelSeconds = pj.value("travelSeconds", 2.0f);
                    float holdSeconds = pj.value("holdSeconds", 0.0f);
                    cameraPath->AddPoint(pos, lookAt, travelSeconds, holdSeconds);
                }
            }

            if (scene) {
                scene->RegisterAnimator(node);
            }
        }
    }
}

// Called once after a full node tree has been deserialized (by LoadScene,
// DuplicateNode, or DeserializeNodeFromString) to resolve every pending
// AnimationComponent's node-index map and every pending SkinComponent's joint
// list, now that the whole subtree actually exists. Pre-order numbering is
// rebuilt per animated-subtree root, matching how GLTFLoader numbered them
// originally (root = 0, then children depth-first).
void ResolvePendingAnimationData() {
    for (TNode* animRoot : g_pendingAnimationRoots) {
        auto* anim = animRoot->GetComponent<AnimationComponent>();
        if (!anim) continue;

        std::unordered_map<int, TNode*> preOrderToNode;
        int counter = 0;
        std::function<void(TNode*)> walk = [&](TNode* n) {
            preOrderToNode[counter++] = n;
            for (TNode* child : n->children) walk(child);
        };
        walk(animRoot);

        anim->SetNodeIndexMap(preOrderToNode);

        for (auto& pending : g_pendingSkins) {
            if (!pending.meshNode) continue; // already resolved under another root

            // A skin's mesh node belongs to this animated subtree if it's
            // reachable from animRoot -- check via the pre-order map we just built.
            bool inThisSubtree = false;
            for (const auto& [idx, n] : preOrderToNode) {
                if (n == pending.meshNode) { inThisSubtree = true; break; }
            }
            if (!inThisSubtree) continue;

            std::vector<TNode*> joints;
            joints.reserve(pending.jointPreOrderIndices.size());
            for (int idx : pending.jointPreOrderIndices) {
                auto it = preOrderToNode.find(idx);
                joints.push_back(it != preOrderToNode.end() ? it->second : nullptr);
            }
            pending.meshNode->AddComponent<SkinComponent>(std::move(joints), std::move(pending.inverseBindMatrices));
            pending.meshNode = nullptr;
        }
    }

    g_pendingAnimationRoots.clear();
    g_pendingSkins.clear();
}

json SerializeNode(const TNode* node) {
    json j;

    // Serialize name
    if (!node->name.empty()) {
        j["name"] = node->name;
    }

    // Serialize transform
    j["transform"] = SerializeTransform(node->transform);

    if (!node->visible) {
        j["visible"] = false;
    }

    if (node->locked) {
        j["locked"] = true;
    }

    // Serialize bounding volume
    if (node->boundingBox) {
        j["boundingBox"] = SerializeBoundingVolume(node->boundingBox);
    }

    // Serialize components
    json components = SerializeComponents(node);
    if (!components.empty()) {
        j["components"] = components;
    }

    // Serialize children
    if (!node->children.empty()) {
        json children_array = json::array();
        for (const TNode* child : node->children) {
            children_array.push_back(SerializeNode(child));
        }
        j["children"] = children_array;
    }

    return j;
}

TNode* DeserializeNode(const json& j, Scene* scene) {
    BoundingVolume* boundingBox = nullptr;
    if (j.contains("boundingBox") && !j["boundingBox"].empty()) {
        boundingBox = DeserializeBoundingVolume(j["boundingBox"]);
    }

    std::string nodeName;
    if (j.contains("name")) {
        nodeName = j["name"];
    }

    TNode* node = new TNode(boundingBox, nodeName);

    // Deserialize transform
    if (j.contains("transform")) {
        node->transform = DeserializeTransform(j["transform"]);
    }

    if (j.contains("visible")) {
        node->visible = j["visible"];
    }

    if (j.contains("locked")) {
        node->locked = j["locked"];
    }

    // Deserialize components
    if (j.contains("components")) {
        DeserializeComponents(node, scene, j["components"]);
    }

    // Deserialize children
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& child_j : j["children"]) {
            TNode* child = DeserializeNode(child_j, scene);
            if (child) {
                node->addChild(child);
            }
        }
    }

    return node;
}

} // namespace

bool SceneSerializer::SaveScene(Scene* scene, const std::string& filePath) {
    if (!scene) {
        Log::Error("Cannot save null scene");
        return false;
    }

    try {
        json j;
        j["version"] = 1;

        j["showGrid"] = scene->IsGridVisible();
        const glm::vec3& clearColor = scene->GetClearColor();
        j["clearColor"] = {clearColor.r, clearColor.g, clearColor.b};

        if (Skybox* skybox = scene->GetSkybox()) {
            if (!skybox->GetName().empty()) {
                j["skybox"] = skybox->GetName();
            }
        }

        TNode* root = scene->GetRoot();
        if (root) {
            j["root"] = SerializeNode(root);
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            Log::Error("Failed to open file for writing: " + filePath);
            return false;
        }

        file << j.dump(2);
        file.close();

        Log::Info("Scene saved to: " + filePath);
        return true;
    } catch (const std::exception& e) {
        Log::Error(std::string("Error saving scene: ") + e.what());
        return false;
    }
}

Scene* SceneSerializer::LoadScene(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            Log::Error("Failed to open file for reading: " + filePath);
            return nullptr;
        }

        json j;
        file >> j;
        file.close();

        Scene* scene = new Scene();
        scene->Init();

        if (j.contains("showGrid")) {
            scene->SetGridVisible(j["showGrid"]);
        }
        if (j.contains("clearColor") && j["clearColor"].is_array()) {
            auto c = j["clearColor"];
            scene->SetClearColor(glm::vec3(c[0], c[1], c[2]));
        }
        if (j.contains("skybox")) {
            std::string skyboxFolder = j["skybox"];
            auto cubemap = ResourceManager::LoadSkyboxFromFolder(skyboxFolder);
            if (cubemap) {
                scene->SetSkybox(std::make_shared<Skybox>(cubemap, skyboxFolder));
            } else {
                Log::Error("SceneSerializer: failed to reload skybox '" + skyboxFolder + "'");
            }
        }

        if (j.contains("root")) {
            TNode* root = DeserializeNode(j["root"], scene);
            if (root) {
                scene->GetRoot()->addChild(root);
            }
        }
        ResolvePendingAnimationData();

        Log::Info("Scene loaded from: " + filePath);
        return scene;
    } catch (const std::exception& e) {
        Log::Error(std::string("Error loading scene: ") + e.what());
        return nullptr;
    }
}

TNode* SceneSerializer::DuplicateNode(const TNode* node, Scene* scene) {
    if (!node) return nullptr;

    try {
        json j = SerializeNode(node);
        TNode* result = DeserializeNode(j, scene);
        ResolvePendingAnimationData();
        return result;
    } catch (const std::exception& e) {
        Log::Error(std::string("Error duplicating node: ") + e.what());
        return nullptr;
    }
}

std::string SceneSerializer::SerializeNodeToString(const TNode* node) {
    if (!node) return "";
    return SerializeNode(node).dump();
}

TNode* SceneSerializer::DeserializeNodeFromString(const std::string& jsonStr, Scene* scene) {
    try {
        json j = json::parse(jsonStr);
        TNode* result = DeserializeNode(j, scene);
        ResolvePendingAnimationData();
        return result;
    } catch (const std::exception& e) {
        Log::Error(std::string("Error pasting node: ") + e.what());
        return nullptr;
    }
}
