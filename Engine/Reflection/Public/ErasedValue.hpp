#pragma once

#include <cstddef>
#include <memory>
#include <new>

#include "Assert.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"
#include "TypeOps.hpp"

namespace Crowy
{
    // one default-constructed value of an erased type, in memory sized and
    // aligned for it; the value and its type travel together
    class ErasedValue {
    private:
        struct Free {
            usize align = 0;

            void operator()(std::byte* memory) const noexcept {
                ::operator delete(memory, std::align_val_t{align});
            }
        };

        const TypeOps& ops;
        std::unique_ptr<std::byte[], Free> memory;

    public:
        ~ErasedValue() { ops.destroy(memory.get()); }
        CROWY_DECLARE_PINNED(ErasedValue)

        explicit ErasedValue(const TypeOps& ops)
            : ops(ops),
              memory(
                  static_cast<std::byte*>(
                      ::operator new(ops.size, std::align_val_t{ops.align})
                  ),
                  Free{ops.align}
              ) {
            CROWY_ASSERT(
                ops.construct != nullptr && ops.destroy != nullptr,
                "an erased value of a type with no default constructor"
            );
            ops.construct(memory.get());
        }

        const TypeOps& Ops() const noexcept { return ops; }
        void* Get() const noexcept { return memory.get(); }
    };
}
