#pragma once
#include <memory>

class IRenderer;

class Application
{
public:
    Application();
    ~Application();
    void Run();

private:
    std::unique_ptr<IRenderer> m_renderer;
};
