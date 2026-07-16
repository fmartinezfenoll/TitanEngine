#include "Debug/ImGuiLayoutUtils.h"
#include <imgui.h>

namespace ImGuiLayoutUtils {

void SameLineOrWrap(float nextWidgetWidth, bool isFirstOnLine) {
    if (isFirstOnLine) return;

    float avail = ImGui::GetContentRegionAvail().x;
    if (avail >= nextWidgetWidth) {
        ImGui::SameLine();
    } else {
        // Doesn't fit on the current line -- without an explicit NewLine(),
        // the cursor stays at its residual X from the previous widget and the
        // next one gets drawn clipped against the window's right edge.
        ImGui::NewLine();
    }
}

}
