#pragma once

#include "EnumUtil.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    // one primitive row drawing a mesh row the code added to the RenderScene
    // itself, landing and leaving with the component at Sync
    struct MeshRowComponent {
        MeshHandle mesh;
        PrimitiveFlags flags =
            combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow);
        // the row this component owns, from the Sync it lands at
        PrimitiveHandle row;
    };
}
