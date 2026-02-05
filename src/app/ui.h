#ifndef UI_H
#define UI_H

#include "mainEditor.h"
#include "leftPanel.h"

class Ui
{
public:
    Ui();
    void draw();
private:
    MainEditor m_mainEditor;
    LeftPanel m_leftPanel;
    float m_leftPanelWidth = -1.f;
    float m_rightPanelWidth = -1.f;
    void DrawSplitter(float totalWidth, float thickness, float minLeft, float minRight);
};

#endif