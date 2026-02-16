#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

class IRenderer;

class RendererFactory {
public:
    using Creator = std::function<std::unique_ptr<IRenderer>()>;

    static RendererFactory& Instance();

    void RegisterRenderer(const std::string& name, Creator creator);

    std::unique_ptr<IRenderer> Create(const std::string& name);

private:
    RendererFactory() = default;

    std::unordered_map<std::string, Creator> creators;
};
