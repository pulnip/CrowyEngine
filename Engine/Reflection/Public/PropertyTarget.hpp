#pragma once

#include <functional>
#include <utility>

#include "Primitives.hpp"

namespace Crowy
{
    struct TypeDesc;

    // What a port target or a panel section edits, found again at every
    // access, so a row that moved or went away is never reached through a
    // stale address. A read hands out only a const object, so nothing can
    // write past the write access, which may record the change.
    class PropertyTarget {
    public:
        // nullptr when what the target names is gone
        using Reader = std::function<const void*()>;
        using Writer = std::function<void*()>;

    private:
        Reader reader;
        Writer writer;
        const TypeDesc* desc = nullptr;

    public:
        PropertyTarget(Reader reader, Writer writer, const TypeDesc& desc)
            : reader(std::move(reader)),
              writer(std::move(writer)),
              desc(&desc) {}

        // an object whose address never changes
        static PropertyTarget Fixed(void* object, const TypeDesc& desc) {
            return PropertyTarget(
                [object]() -> const void* { return object; },
                [object] { return object; },
                desc
            );
        }

        const void* Read() const { return reader(); }
        // asked for only at the moment of writing
        void* Write() const { return writer(); }

        const TypeDesc& Desc() const noexcept { return *desc; }
    };
}
