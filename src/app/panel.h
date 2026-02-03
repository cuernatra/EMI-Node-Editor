#ifndef PANEL_H
#define PANEL_H

#include <SFML/Graphics/RenderWindow.hpp>

class Panel 
{
public: 
    virtual void draw() = 0;
};

#endif