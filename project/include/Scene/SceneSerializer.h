#pragma once
#include <string>

class Scene;

class SceneSerializer {
public:
    static bool SaveScene(Scene* scene, const std::string& filePath);
    static Scene* LoadScene(const std::string& filePath);
};
