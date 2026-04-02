#include "linkRenderer.h"

#include "pinRenderer.h"

void DrawLinks(const std::vector<Link>& links)
{
    for (const auto& lnk : links)
    {
        if (!lnk.alive) continue;

        const ImVec4 col = GetPinTypeColor(lnk.type);
        ed::Link(lnk.id, lnk.startPinId, lnk.endPinId,
                 col, /* thickness */ 2.0f);
    }
}
