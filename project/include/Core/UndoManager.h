#pragma once
#include <string>
#include <deque>

class Scene;

// Editor undo/redo built on whole-scene JSON snapshots (SceneSerializer).
// The caller takes a snapshot *before* each mutating action via PushSnapshot;
// Undo/Redo then swap the current scene state with the top of the respective
// stack. The undo stack is bounded by EngineSettings::GetUndoHistoryLimit()
// (oldest entries dropped once exceeded). All state is static -- there is one
// shared history for the editor session.
class UndoManager {
public:
    // Call right before an action that changes the scene. Captures the current
    // scene state onto the undo stack and clears the redo stack (a fresh action
    // invalidates any redo history, as in every editor).
    static void PushSnapshot(Scene* scene);

    // Restore the previous / next state. No-op (returns false) when the
    // corresponding stack is empty. On success the scene is rebuilt in-place.
    static bool Undo(Scene* scene);
    static bool Redo(Scene* scene);

    static bool CanUndo() { return !undoStack.empty(); }
    static bool CanRedo() { return !redoStack.empty(); }

    // Drops all history -- call when loading/switching scenes so undo can't
    // cross into an unrelated scene's snapshots.
    static void Clear();

private:
    static inline std::deque<std::string> undoStack;
    static inline std::deque<std::string> redoStack;
};
