#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include "Resource.h"
#include "Core/Log.h"

class ResourceManager
{
public:

    template<typename T, typename... Args>
    T* Create(const std::string& name, Args&&... args)
    {
        static_assert(std::is_base_of<Resource, T>::value,
            "T must inherit from Resource");

        auto& typeMap = resources[typeid(T)];

        auto it = typeMap.find(name);
        if (it != typeMap.end())
        {
            Log::Info("Reusing resource: " +
                      std::string(typeid(T).name()) +
                      " : " + name);

            return static_cast<T*>(it->second.get());
        }

        std::unique_ptr<T> resource =
            std::make_unique<T>(name, std::forward<Args>(args)...);

        T* raw = resource.get();
        typeMap[name] = std::move(resource);

        Log::Info("Created resource: " +
                  std::string(typeid(T).name()) +
                  " : " + name);

        return raw;
    }

    template<typename T>
    T* Get(const std::string& name)
    {
        auto typeIt = resources.find(typeid(T));
        if (typeIt == resources.end())
        {
            Log::Error("Resource type not found: " +
                       std::string(typeid(T).name()));
            return nullptr;
        }

        auto& typeMap = typeIt->second;
        auto it = typeMap.find(name);

        if (it == typeMap.end())
        {
            Log::Error("Resource not found: " +
                       std::string(typeid(T).name()) +
                       " : " + name);
            return nullptr;
        }

        return static_cast<T*>(it->second.get());
    }

    void Clear()
    {
        Log::Info("Clearing all resources");
        resources.clear();
    }

private:

    using ResourceMap =
        std::unordered_map<std::string, std::unique_ptr<Resource>>;

    std::unordered_map<std::type_index, ResourceMap> resources;
};
