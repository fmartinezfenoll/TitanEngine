#pragma once

#include <string>
#include <vector>
#include <memory>

class TNode;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool Init(int width, int height, const std::string& appName) = 0;
    virtual void Shutdown() = 0;


    virtual void* GetWindow() const { return nullptr; }
    virtual bool ShouldClose() const { return false; }

    virtual void PollEvents() = 0;
    virtual void BeginFrame() = 0;
    virtual void Render() = 0;
    virtual void EndFrame() = 0;
    void SetEntitiesReference(std::vector<TNode*>* entitiesRef);

protected:
    std::vector<TNode*>* entities = nullptr;
};
