#include "editorLayer.h"
#include "appConstants.h"
#include <imgui.h>
#include <string>

void EditorLayer::draw() {
    std::string stateText;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(
        ImVec2(appConstants::panelWidth, ImGui::GetIO().DisplaySize.y)
    );

    ImGui::Begin(
        "Controlpanel",
        nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse
    );

    ImGui::PushStyleColor(
        ImGuiCol_Button,
        active ? appConstants::GREEN : appConstants::GRAY
    );

    if (ImGui::Button("Test button")) {
        active = !active;
    }

    ImGui::PopStyleColor();

    if (active) stateText = "ON";
    else stateText = "OFF";

    ImGui::Text("State: %s", stateText);

    ImGui::End();
}