#pragma once
#include <string>

class Resource
{
public:
    explicit Resource(const std::string& name)
        : Name(name) {}

    virtual ~Resource() = default;

    const std::string& GetName() const
    {
        return Name;
    }

protected:
    std::string Name;
};