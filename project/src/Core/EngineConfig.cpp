#include "Core/EngineConfig.h"
#include "Core/EngineSettings.h"
#include "Core/Log.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

std::unordered_map<std::string, std::string> ParseIni(const std::string& path)
{
    std::unordered_map<std::string, std::string> values;
    std::ifstream file(path);
    if (!file) return values;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        values[key] = value;
    }
    return values;
}

bool ToBool(const std::string& s, bool fallback)
{
    if (s == "1" || s == "true") return true;
    if (s == "0" || s == "false") return false;
    return fallback;
}

} // namespace

void EngineConfig::Load(const std::string& path)
{
    auto values = ParseIni(path);
    if (values.empty())
    {
        Log::Info("No engine.ini found, using default settings");
        return;
    }

    auto it = values.find("VSync");
    if (it != values.end())
        EngineSettings::SetVSyncEnabled(ToBool(it->second, EngineSettings::IsVSyncEnabled()));

    it = values.find("FrustumCulling");
    if (it != values.end())
        EngineSettings::SetFrustumCullingEnabled(ToBool(it->second, EngineSettings::IsFrustumCullingEnabled()));

    it = values.find("DistanceCull");
    if (it != values.end())
        EngineSettings::SetDistanceCullEnabled(ToBool(it->second, EngineSettings::IsDistanceCullEnabled()));

    it = values.find("MaxDrawDistance");
    if (it != values.end())
        EngineSettings::SetMaxDrawDistance(std::stof(it->second));

    it = values.find("Shadows");
    if (it != values.end())
        EngineSettings::SetShadowsEnabled(ToBool(it->second, EngineSettings::AreShadowsEnabled()));

    it = values.find("IBL");
    if (it != values.end())
        EngineSettings::SetIBLEnabled(ToBool(it->second, EngineSettings::IsIBLEnabled()));

    it = values.find("ShadowResolution2D");
    if (it != values.end())
        EngineSettings::SetShadowResolution2D(std::stoi(it->second));

    it = values.find("ShadowResolutionCube");
    if (it != values.end())
        EngineSettings::SetShadowResolutionCube(std::stoi(it->second));

    it = values.find("DirectionalShadowBoxSize");
    if (it != values.end())
        EngineSettings::SetDirectionalShadowBoxSize(std::stof(it->second));

    it = values.find("AlwaysSnap");
    if (it != values.end())
        EngineSettings::SetAlwaysSnapEnabled(ToBool(it->second, EngineSettings::IsAlwaysSnapEnabled()));

    it = values.find("PositionSnap");
    if (it != values.end())
        EngineSettings::SetPositionSnap(std::stof(it->second));

    it = values.find("RotationSnapDegrees");
    if (it != values.end())
        EngineSettings::SetRotationSnapDegrees(std::stof(it->second));

    it = values.find("ScaleSnap");
    if (it != values.end())
        EngineSettings::SetScaleSnap(std::stof(it->second));

    it = values.find("AutoSave");
    if (it != values.end())
        EngineSettings::SetAutoSaveEnabled(ToBool(it->second, EngineSettings::IsAutoSaveEnabled()));

    it = values.find("AutoSaveIntervalSeconds");
    if (it != values.end())
        EngineSettings::SetAutoSaveIntervalSeconds(std::stof(it->second));

    it = values.find("LastActiveScene");
    if (it != values.end())
        EngineSettings::SetLastActiveScene(it->second);

    it = values.find("UndoHistoryLimit");
    if (it != values.end())
        EngineSettings::SetUndoHistoryLimit(std::stoi(it->second));

    Log::Info("Loaded engine settings from " + path);
}

void EngineConfig::Save(const std::string& path)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        Log::Error("Failed to write engine config: " + path);
        return;
    }

    file << "VSync=" << (EngineSettings::IsVSyncEnabled() ? 1 : 0) << "\n";
    file << "FrustumCulling=" << (EngineSettings::IsFrustumCullingEnabled() ? 1 : 0) << "\n";
    file << "DistanceCull=" << (EngineSettings::IsDistanceCullEnabled() ? 1 : 0) << "\n";
    file << "MaxDrawDistance=" << EngineSettings::GetMaxDrawDistance() << "\n";
    file << "Shadows=" << (EngineSettings::AreShadowsEnabled() ? 1 : 0) << "\n";
    file << "IBL=" << (EngineSettings::IsIBLEnabled() ? 1 : 0) << "\n";
    file << "ShadowResolution2D=" << EngineSettings::GetShadowResolution2D() << "\n";
    file << "ShadowResolutionCube=" << EngineSettings::GetShadowResolutionCube() << "\n";
    file << "DirectionalShadowBoxSize=" << EngineSettings::GetDirectionalShadowBoxSize() << "\n";
    file << "AlwaysSnap=" << (EngineSettings::IsAlwaysSnapEnabled() ? 1 : 0) << "\n";
    file << "PositionSnap=" << EngineSettings::GetPositionSnap() << "\n";
    file << "RotationSnapDegrees=" << EngineSettings::GetRotationSnapDegrees() << "\n";
    file << "ScaleSnap=" << EngineSettings::GetScaleSnap() << "\n";
    file << "AutoSave=" << (EngineSettings::IsAutoSaveEnabled() ? 1 : 0) << "\n";
    file << "AutoSaveIntervalSeconds=" << EngineSettings::GetAutoSaveIntervalSeconds() << "\n";
    file << "LastActiveScene=" << EngineSettings::GetLastActiveScene() << "\n";
    file << "UndoHistoryLimit=" << EngineSettings::GetUndoHistoryLimit() << "\n";
}
