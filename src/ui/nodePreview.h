/**
 * @file nodePreview.h
 * @brief Mini node preview renderer for the palette
 * 
 * Renders a small visual representation of a node type in the left panel.
 * Shows the node structure (pins, fields) at a miniature scale for quick
 * visual identification before dragging to canvas.
 */

#ifndef NODE_PREVIEW_H
#define NODE_PREVIEW_H

#include "../core/graph/types.h"

/**
 * @brief Renders a small preview of a node type
 * 
 * Draws a miniature representation of a node showing:
 * - Node body with title
 * - Input pins on the left (tiny circles)
 * - Output pins on the right (tiny circles)
 * - Field labels if space allows
 * 
 * Used in the left panel palette to show what each draggable
 * node type looks like before user drags it to canvas.
 */
class NodePreview
{
public:
    /**
     * @brief Draw a small preview of a node type
     * 
     * @param nodeType The type of node to preview
     * 
     * Queries the node registry to get pin/field information,
     * then draws a scaled-down version. Safe to call repeatedly.
     */
    static void Draw(NodeType nodeType);
};

#endif // NODE_PREVIEW_H
