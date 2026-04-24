#pragma once
#include <string>

class Resource
{
public:
    explicit Resource(const std::string& name)
        : m_Name(name) {}

    virtual ~Resource() = default;

    const std::string& GetName() const
    {
        return m_Name;
    }

protected:
    std::string m_Name;
};