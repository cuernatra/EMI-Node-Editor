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

void TopPanel::setPreviewCallback(std::function<void()> cb)
{
    m_previewCallback = std::move(cb);
}

void TopPanel::draw()
{   
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));

    ImGui::PushStyleColor(ImGuiCol_Button, colors::gray);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::lightGray);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::gray);

    if (ImGui::Button("Filesystem", ImVec2(95, m_height - 14)))
    {
        if (m_filesystemCallback)
            m_filesystemCallback();
    }

    ImGui::SameLine(0, 10);

    if (ImGui::Button("Settings", ImVec2(85, m_height - 14)))
    {
        if (m_settingsCallback)
            m_settingsCallback();
    }

    ImGui::SameLine(0, 10);

    if (ImGui::Button("Preview", ImVec2(85, m_height - 14)))
    {
        if (m_previewCallback)
            m_previewCallback();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}