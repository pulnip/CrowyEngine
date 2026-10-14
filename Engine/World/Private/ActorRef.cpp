#include "ActorRef.hpp"

#include "Assert.hpp"

namespace
{
    thread_local const Crowy::ActorRefScope* currentScope = nullptr;
}

namespace Crowy
{
    ActorRefScope::~ActorRefScope() {
        CROWY_ASSERT(currentScope == this, "scopes end in reverse order");
        currentScope = outer;
    }

    ActorRefScope::ActorRefScope()
        : outer(currentScope) {
        currentScope = this;
    }

    const ActorRefScope* ActorRefScope::Current() noexcept {
        return currentScope;
    }

    void ActorRefScope::Bind(i64 fileId, ActorId actor) {
        CROWY_ASSERT(actor.IsValid(), "binding an id to no actor");

        [[maybe_unused]] const bool newId =
            actors.try_emplace(fileId, actor).second;
        [[maybe_unused]] const bool newActor =
            fileIds.try_emplace(actor, fileId).second;
        CROWY_ASSERT(newId && newActor, "an id or an actor bound twice");
    }

    std::optional<i64> ActorRefScope::FileIdOf(ActorId actor) const {
        const auto it = fileIds.find(actor);
        return it == fileIds.end() ? std::nullopt : std::optional(it->second);
    }

    std::optional<ActorId> ActorRefScope::ActorOf(i64 fileId) const {
        const auto it = actors.find(fileId);
        return it == actors.end() ? std::nullopt : std::optional(it->second);
    }

    bool TypeTraits<ActorRef>::deserialize(
        void* data,
        const DOM::Value& value
    ) {
        auto& ref = *static_cast<ActorRef*>(data);
        if(value.is_none()) {
            ref = ActorRef{};
            return true;
        }

        if(const auto* scope = ActorRefScope::Current()) {
            const auto id = value.asInt();
            const auto actor = id ? scope->ActorOf(*id) : std::nullopt;
            if(!actor)
                return false;

            ref = ActorRef{*actor};
            return true;
        }

        const auto* pair = value.asArray();
        if(pair == nullptr || pair->size() != 2)
            return false;

        const auto index = (*pair)[0].asInt();
        const auto generation = (*pair)[1].asInt();
        if(!index || !generation || *index < 0 || *generation < 0)
            return false;

        ref = ActorRef{
            ActorId(static_cast<usize>(*index), static_cast<usize>(*generation))
        };
        return true;
    }

    void TypeTraits<ActorRef>::serialize(const void* data, DOM::Value& out) {
        const auto& ref = *static_cast<const ActorRef*>(data);
        // an actor the file does not hold is no actor to it
        if(const auto* scope = ActorRefScope::Current()) {
            const auto id = scope->FileIdOf(ref.actor);
            out = id ? DOM::Value(*id) : DOM::Value();
            return;
        }

        if(!ref.actor.IsValid()) {
            out = DOM::Value();
            return;
        }

        DOM::Array pair;
        pair.emplace_back(static_cast<i64>(ref.actor.GetIndex()));
        pair.emplace_back(static_cast<i64>(ref.actor.GetGeneration()));
        out = DOM::Value(std::move(pair));
    }
}
