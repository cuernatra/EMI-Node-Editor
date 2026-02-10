#ifndef MAINEDITOR_H
#define MAINEDITOR_H

#include "constants.h"
#include "imgui_node_editor.h"

/**
 * @brief Represent node-editor.
 * Uses imgui_node_editor library for node editor functionality.
 *
 * @author Joel Turkka
 */
class MainEditor
{
public:
    MainEditor();
    ~MainEditor();
    void draw();

private:
    ax::NodeEditor::EditorContext* m_ctx = nullptr;
    bool m_firstFrame = true;
};

#endif