#ifndef PANEL_H
#define PANEL_H

#include <SFML/Graphics/RenderWindow.hpp>

struct Position
{
    float x;
    float y;
};

class Panel 
{
public: 
    virtual void draw() = 0;
};

#endif