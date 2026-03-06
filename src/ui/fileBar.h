#ifndef FILEBAR_H
#define FILEBAR_H

#include "../app/constants.h"
#include <vector>

/**
 * @brief Represents the file bar,
 * which contains menu file. Under it are items save, load
 * and exit.
 * @author Alex Lundström
 */
class FileBar
{
public: 
    FileBar();
    void draw();
    
private:
    float m_height;
};

#endif
