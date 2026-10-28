#pragma once

#include <optional>
#include <unordered_map>

#include "Actor.hpp"
#include "Semantics.hpp"
#include "TypeOps.hpp"

namespace Crowy
{
    // another actor: the scene file's id inside an ActorRefScope, the live
    // [index, generation] anywhere else
    struct ActorRef {
        ActorId actor;

        bool operator==(const ActorRef&) const noexcept = default;
    };

    // The ids one scene file gives its actors, in force on this thread for
    // the scope's life; a load binds them before it applies any component.
    class ActorRefScope {
    private:
        using FileIds = std::unordered_map<ActorId, i64>;
        using ActorsById = std::unordered_map<i64, ActorId>;

        FileIds fileIds;
        ActorsById actors;
        const ActorRefScope* outer = nullptr;

    public:
        ~ActorRefScope();
        CROWY_DECLARE_PINNED(ActorRefScope)

        ActorRefScope();

        // the innermost scope on this thread, or nullptr
        static const ActorRefScope* Current() noexcept;

        // one actor per id and one id per actor
        void Bind(i64 fileId, ActorId actor);
        std::optional<i64> FileIdOf(ActorId actor) const;
        std::optional<ActorId> ActorOf(i64 fileId) const;
    };

    template<>
    struct TypeTraits<ActorRef> {
        static constexpr CStr name = "ActorRef";
        // null, or the scope's id, or [index, generation] outside one
        static bool deserialize(void*, const DOM::Value&);
        static void serialize(const void*, DOM::Value&);
    };
}
