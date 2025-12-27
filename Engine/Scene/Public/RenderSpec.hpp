#pragma once

#include <string>
#include <vector>

namespace Crowy
{
    struct ShaderSpec{
        std::string vsFilePath;
        std::string vsFuncName;
        std::string fsFilePath;
        std::string fsFuncName;
    };

    struct RenderPassSpec{
        std::string name;
        std::vector<std::string> targets;
        ShaderSpec shader;

        // DepthSpec depth;
        // BlendSpec blend;
        // RasterizerSpec rasterizer;
    };

    struct RenderSpec{
        std::vector<RenderPassSpec> passes;
    };
}