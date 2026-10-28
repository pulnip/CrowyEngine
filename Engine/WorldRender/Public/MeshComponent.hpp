#pragma once

#include "EnumUtil.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    // one primitive row of the RenderScene the World provides, landing and
    // leaving with the component at Sync
    struct MeshComponent {
        MeshHandle mesh;
        PrimitiveFlags flags =
            combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow);
        // the row this component owns, from the Sync it lands at
        PrimitiveHandle row;
    };
}
