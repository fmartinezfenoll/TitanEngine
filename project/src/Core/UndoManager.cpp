#include "Core/UndoManager.h"
#include "Core/EngineSettings.h"
#include "Scene/SceneSerializer.h"
#include "Scene/Scene.h"

void UndoManager::PushSnapshot(Scene* scene) {
    if (!scene) return;

    std::string snapshot = SceneSerializer::SerializeSceneToString(scene);
    if (snapshot.empty()) return;

    // Collapse consecutive identical snapshots (e.g. an action that ended up
    // not changing anything) so undo doesn't waste a step on a no-op.
    if (!undoStack.empty() && undoStack.back() == snapshot) {
        redoStack.clear();
        return;
    }

    undoStack.push_back(std::move(snapshot));
    redoStack.clear();

    int limit = EngineSettings::GetUndoHistoryLimit();
    while (static_cast<int>(undoStack.size()) > limit) {
        undoStack.pop_front();
    }
}

bool UndoManager::Undo(Scene* scene) {
    if (!scene || undoStack.empty()) return false;

    // Current state goes onto the redo stack so a subsequent Redo can return here.
    std::string current = SceneSerializer::SerializeSceneToString(scene);
    if (!current.empty()) {
        redoStack.push_back(std::move(current));
    }

    std::string previous = std::move(undoStack.back());
    undoStack.pop_back();
    return SceneSerializer::RestoreSceneFromString(previous, scene);
}

bool UndoManager::Redo(Scene* scene) {
    if (!scene || redoStack.empty()) return false;

    std::string current = SceneSerializer::SerializeSceneToString(scene);
    if (!current.empty()) {
        undoStack.push_back(std::move(current));
    }

    std::string next = std::move(redoStack.back());
    redoStack.pop_back();
    return SceneSerializer::RestoreSceneFromString(next, scene);
}

void UndoManager::Clear() {
    undoStack.clear();
    redoStack.clear();
}
