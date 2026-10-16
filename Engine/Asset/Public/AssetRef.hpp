#pragma once

#include <optional>

#include "Primitives.hpp"
#include "TypeOps.hpp"

namespace Crowy
{
    enum class AssetMount : u8 {
        Content,
        Engine,
    };

    // Names one asset as a mount and a path under it. Parse keeps the path
    // normalized, so one asset has one spelling; the default names none.
    class AssetRef {
    private:
        AssetMount mount = AssetMount::Content;
        Str path;

    public:
        // "engine:" names the engine mount, no prefix the content mount
        static std::optional<AssetRef> Parse(StrView text);

        bool IsNone() const noexcept { return path.empty(); }
        AssetMount Mount() const noexcept { return mount; }
        const Str& Path() const noexcept { return path; }
        Str ToString() const;

        bool operator==(const AssetRef&) const = default;
    };

    template<>
    struct TypeTraits<AssetRef> {
        static constexpr CStr name = "AssetRef";
        // null, or the text Parse reads
        static bool deserialize(void*, const DOM::Value&);
        static void serialize(const void*, DOM::Value&);
    };
}
