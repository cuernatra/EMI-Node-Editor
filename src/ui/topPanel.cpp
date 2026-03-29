#include "topPanel.h"

TopPanel::TopPanel() : m_height{elementSizes::topBarHeight}
{
}

void TopPanel::setFilesystemCallback(std::function<void()> cb)
{
    m_filesystemCallback = std::move(cb);
}

void TopPanel::setSettingsCallback(std::function<void()> cb)
{
    m_settingsCallback = std::move(cb);
}

void TopPanel::setPreviewCallback(std::function<void(bool)> cb)
{
    m_previewCallback = std::move(cb);
}

void TopPanel::draw()
{   
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    const float buttonHeight = m_height - 14.0f;
    ImGui::SetCursorPosY((m_height - buttonHeight) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, colors::background);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::elevated);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::elevated);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(colors::textSecondary.x, colors::textSecondary.y, colors::textSecondary.z, 0.45f));

    if (ImGui::Button("Filesystem", ImVec2(95, buttonHeight)))
    {
        if (m_filesystemCallback)
            m_filesystemCallback();
    }

    ImGui::SameLine(0, 10);

    if (ImGui::Button("Settings", ImVec2(85, buttonHeight)))
    {
        if (m_settingsCallback)
            m_settingsCallback();
    }

    ImGui::SameLine(0, 10);

    const bool previewWasEnabled = m_previewEnabled;

    if (previewWasEnabled)
    {
        // Show toggled state as "pressed" by using active-like colors.
        ImGui::PushStyleColor(ImGuiCol_Button, colors::surface);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::elevated);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::background);
    }

    if (ImGui::Button("Preview", ImVec2(85, buttonHeight)))
    {
        m_previewEnabled = !m_previewEnabled;
        if (m_previewCallback)
            m_previewCallback(m_previewEnabled);
    }

    // Pop using the pre-click state so style stack always stays balanced
    // even when the click toggles m_previewEnabled.
    if (previewWasEnabled)
    {
        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);
}