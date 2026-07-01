#pragma once
#include <string>
#include <vector>

class TNode;

class GLTFLoader {
public:
    static std::vector<TNode*> LoadModel(const std::string& path);
};
