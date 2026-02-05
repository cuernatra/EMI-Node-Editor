#ifndef MAINEDITOR_H
#define MAINEDITOR_H

#include "constants.h"

/**
 * @brief Represent node-editor
 *
 * @author Joel Turkka
 */
class MainEditor
{
public: 
    MainEditor();
    void draw();
    const float getWidth() const;
    
private:
    float m_width;
    float m_height;
    Position m_position;
};

#endif
