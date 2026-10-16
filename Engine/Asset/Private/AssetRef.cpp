#include "AssetRef.hpp"

#include <format>
#include <ranges>
#include <vector>

namespace
{
    constexpr Crowy::StrView EnginePrefix = "engine";

    // Split by hand: std::filesystem turns '/' into '\' on Windows.
    std::optional<Crowy::Str> normalizePath(Crowy::StrView text) {
        using namespace Crowy;

        // a backslash is a name character on POSIX, so it has no one meaning
        if(text.starts_with('/') || text.contains('\\') || text.contains(':'))
            return std::nullopt;

        std::vector<StrView> names;
        for(const auto part: text | std::views::split('/')) {
            const StrView name(part.begin(), part.end());
            if(name.empty() || name == ".")
                continue;

            if(name == "..") {
                if(names.empty())
                    return std::nullopt;

                names.pop_back();
                continue;
            }
            names.push_back(name);
        }
        if(names.empty())
            return std::nullopt;

        Str path;
        for(const auto name: names) {
            if(!path.empty())
                path += '/';
            path += name;
        }
        return path;
    }
}

namespace Crowy
{
    std::optional<AssetRef> AssetRef::Parse(StrView text) {
        AssetRef ref;

        if(const auto colon = text.find(':'); colon != StrView::npos) {
            if(text.substr(0, colon) != EnginePrefix)
                return std::nullopt;

            ref.mount = AssetMount::Engine;
            text.remove_prefix(colon + 1);
        }

        auto path = normalizePath(text);
        if(!path)
            return std::nullopt;

        ref.path = std::move(*path);
        return ref;
    }

    Str AssetRef::ToString() const {
        if(mount == AssetMount::Engine)
            return std::format("{}:{}", EnginePrefix, path);

        return path;
    }

    bool TypeTraits<AssetRef>::deserialize(
        void* data,
        const DOM::Value& value
    ) {
        auto& ref = *static_cast<AssetRef*>(data);
        if(value.is_none()) {
            ref = AssetRef{};
            return true;
        }

        const auto* text = value.asString();
        auto parsed = text ? AssetRef::Parse(*text) : std::nullopt;
        if(!parsed)
            return false;

        ref = std::move(*parsed);
        return true;
    }

    void TypeTraits<AssetRef>::serialize(const void* data, DOM::Value& out) {
        const auto& ref = *static_cast<const AssetRef*>(data);
        out = ref.IsNone() ? DOM::Value() : DOM::Value(ref.ToString());
    }
}
