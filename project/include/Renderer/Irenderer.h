#pragma once

#include <string>
#include <vector>
#include <memory>

class TNodo;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool Init(int width, int height, const std::string& appName) = 0;
    virtual void Shutdown() = 0;
    virtual void Render() = 0;

    virtual void ImGuiNewFrame() {}
    virtual void* GetWindow() const { return nullptr; }

    void SetEntitiesReference(std::vector<TNodo*>* entitiesRef);

protected:
    std::vector<TNodo*>* entities = nullptr;
};
