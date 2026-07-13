#pragma once
#include <string>

// Persists EngineSettings toggles/values to a plain key=value .ini file
// so quality/performance preferences survive between runs.
class EngineConfig
{
public:
    static void Load(const std::string& path = "engine.ini");
    static void Save(const std::string& path = "engine.ini");
};
