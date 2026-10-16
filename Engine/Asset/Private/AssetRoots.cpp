#include "AssetRoots.hpp"

#include "Assert.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    std::filesystem::path resolveAssetPath(
        const AssetRoots& roots,
        const AssetRef& ref
    ) {
        CROWY_ASSERT(!ref.IsNone(), "resolving a ref that names no asset");

        const auto& root =
            ref.Mount() == AssetMount::Engine ? roots.engine : roots.content;
        return root / toPath(ref.Path().c_str());
    }
}
