#ifndef FILEBAR_H
#define FILEBAR_H

#include "../app/constants.h"
#include <vector>
#include "imgui.h"

/**
 * @brief Represents the file bar,
 * which contains menu file. Under it are items save, load
 * and exit.
 * @author Alex Lundström
 */

class MainEditor;
class FileBar
{
public: 
    FileBar(MainEditor* editor);
    void draw();
    
private:
    float m_height;
    //Reference to the main editor, used for save and load actions
    MainEditor* m_editor;
};

#endif
