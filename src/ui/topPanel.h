/**
 * @brief Top toolbar panel for the editor
 * Displays application title and status information.
 */

#ifndef TOPPANEL_H
#define TOPPANEL_H

#include "../app/constants.h"
#include <vector>
#include <functional>

/**
 * @brief Top toolbar panel
 * Fixed-height toolbar at the top of the application window.
 */
class TopPanel
{
public:
    /// Initialize the top panel
    TopPanel();
    
    /// Render the top panel (called every frame).
    void draw();

    void setFilesystemCallback(std::function<void()> cb);
    void setSettingsCallback(std::function<void()> cb);
    void setPreviewCallback(std::function<void(bool)> cb);

    /// Whether preview-on-compile mode is enabled.
    bool isPreviewEnabled() const { return m_previewEnabled; }
    
private:
    /// Height of the toolbar in pixels
    float m_height;
    std::function<void()> m_filesystemCallback;
    std::function<void()> m_settingsCallback;
    std::function<void(bool)> m_previewCallback;
    bool m_previewEnabled = false;
};

#endif
