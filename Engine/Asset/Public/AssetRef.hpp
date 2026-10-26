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

    class AssetRef {
    private:
        AssetMount mount = AssetMount::Content;
        Str path;

    public:
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
        static bool deserialize(void*, const DOM::Value&);
        static void serialize(const void*, DOM::Value&);
    };
}
