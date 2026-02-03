// editorLayer.h - drawing the editor layer UI

#ifndef EDITORLAYER_H
#define EDITORLAYER_H

/**
 * @brief Manages and renders the editor layer UI
 *
 * @author Joel Turkka
 */
class EditorLayer {
public:
    void draw();

private:
    bool active = false;
};

#endif