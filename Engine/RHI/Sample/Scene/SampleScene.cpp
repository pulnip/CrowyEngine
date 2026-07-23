#include "SampleScene.hpp"

namespace Crowy
{
    std::array<std::filesystem::path, 6> SkyboxFacePaths(const std::filesystem::path& dir){
        return {
            dir / "right.ktx2",     // +X
            dir / "left.ktx2",      // -X
            dir / "top.ktx2",       // +Y
            dir / "bottom.ktx2",    // -Y
            dir / "front.ktx2",     // +Z
            dir / "back.ktx2"       // -Z
        };
    }
}
