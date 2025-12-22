#pragma once

#include "Submesh.hpp"
#include "Mesh.hpp"
#include "Material.hpp"
#include "MaterialSet.hpp"
#include "Shader.hpp"

namespace Crowy
{
    struct ResourceHub{
        SubmeshManager     submeshManager;
        MeshManager        meshManager;
        MaterialManager    materialManager;
        MaterialSetManager materialSetManager;
        ShaderManager      shaderManager;
    };
}