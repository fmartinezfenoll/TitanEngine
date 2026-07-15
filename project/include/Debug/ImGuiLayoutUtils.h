#pragma once

// Small shared helpers for keeping hand-rolled ImGui toolbars/rows from
// overflowing a narrowed window, used by DebugUI.cpp and ProjectBrowser.cpp.
namespace ImGuiLayoutUtils {

// Call before drawing a widget that would normally follow ImGui::SameLine():
// if `nextWidgetWidth` doesn't fit in the remaining space on the current
// line, starts a new line instead (no SameLine() called); otherwise calls
// SameLine() as usual. `isFirstOnLine` should be true for the first widget
// in a row (never wraps, never calls SameLine()).
void SameLineOrWrap(float nextWidgetWidth, bool isFirstOnLine);

}
