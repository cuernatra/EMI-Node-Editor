#ifndef MAINEDITOR_H
#define MAINEDITOR_H

#include "editor.h"
#include "constants.h"

/**
 * @brief Represent node-editor
 *
 * @author Joel Turkka
 */
class MainEditor : public Editor
{
public: 
    MainEditor();
    virtual void draw() override;
    
private:
    float m_width;
    float m_height;
    Position m_position;
};

#endif
