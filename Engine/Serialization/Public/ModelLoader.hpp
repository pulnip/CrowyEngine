#pragma once

#include <filesystem>
#include <vector>

#include "Geometry/Overlap3D.hpp"
#include "MeshData.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // the triangles of one material, indexed on their own
    struct ModelSlot {
        // as the file names the material; empty for faces that have none
        Str material;
        MeshData mesh;
        AABB3D bounds;
    };

    // A model in the space of its root, every node's geometry already moved
    // there, so a placement is one world matrix. Slots follow the file's
    // material order, and a material no face uses has none.
    struct ModelData {
        std::vector<ModelSlot> slots;
        AABB3D bounds;
    };

    // Loads an FBX into the convention MeshData documents: the file's axes
    // and handedness become left-handed +Y up, its unit becomes metres,
    // texCoord's origin moves to the top-left, and faces are triangulated
    // with missing normals generated and tangents derived from the UVs.
    // Throws std::runtime_error when the file cannot be read.
    ModelData LoadModel(const std::filesystem::path& path);
}
