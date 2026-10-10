#pragma once

#include "EnumUtil.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    // A mesh drawn at its actor: the World's side of one RenderScene
    // primitive row, which lands and leaves with the component at Sync and
    // is written whole at the Extract after it lands, moves or is written.
    // The World must provide the RenderScene.
    struct MeshComponent {
        MeshHandle mesh;
        PrimitiveFlags flags =
            combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow);
        // the row this component owns, from the Sync it lands at
        PrimitiveHandle row;
    };
}
