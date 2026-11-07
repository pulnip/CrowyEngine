#pragma once

#include <filesystem>
#include <vector>

#include "Geometry/Overlap3D.hpp"
#include "MeshData.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct ModelData;
    struct ModelNode;
    struct ModelSlot;

    using ModelNodes = std::vector<ModelNode>;
    using ModelSlots = std::vector<ModelSlot>;

    // into MeshData's convention, metres; throws std::runtime_error when the
    // file cannot be read
    ModelData loadModel(const std::filesystem::path& path);

    // the triangles of one material, indexed on their own
    struct ModelSlot {
        // as the file names the material; empty for faces that have none
        Str material;
        MeshData mesh;
        AABB3D bounds;
    };

    // one node of the file and its own geometry in its own space, wound as
    // ModelData::slots are: under a mirroring chain, front once placed
    struct ModelNode {
        static constexpr u32 NoParent = ~0u;

        Str name;
        // an index below this node's own
        u32 parent = NoParent;
        Transform local = Transform::Identity();
        ModelSlots slots;
    };

    // every node's geometry moved into the root's space, a slot per material
    // that has faces, in the file's material order
    struct ModelData {
        ModelSlots slots;
        AABB3D bounds;
        // the file's nodes below its root, in preorder
        ModelNodes nodes;
    };
}
