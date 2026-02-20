#pragma once
#include <string>


class Resource
{
public:
    explicit Resource(const std::string& name)
        : name(name) {}

    virtual ~Resource() = default;

    const std::string& GetName() const { return name; }

private:
    std::string name;
};
