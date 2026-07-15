#include "Debug/ImGuiLayoutUtils.h"
#include <imgui.h>

namespace ImGuiLayoutUtils {

void SameLineOrWrap(float nextWidgetWidth, bool isFirstOnLine) {
    if (isFirstOnLine) return;

    float avail = ImGui::GetContentRegionAvail().x;
    if (avail >= nextWidgetWidth) {
        ImGui::SameLine();
    }
    // else: leave the cursor where it is (start of a new line) -- caller's
    // next widget call naturally begins a fresh row.
}

}
