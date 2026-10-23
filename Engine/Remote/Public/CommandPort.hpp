#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "DOM.hpp"
#include "Primitives.hpp"
#include "PropertyTarget.hpp"
#include "PropertyWrite.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    using RequestId = u64;

    class CommandPortImpl;
    struct TypeDesc;

    // One reply per request.
    // Answer inside the handler, or keep the token and answer from a later.
    // A token destroyed unanswered is answered by the port on its next Drain
    // with an error, so a dropped reply is loud rather than a hang.
    // Answering twice is a bug.
    class Reply{
    private:
        std::weak_ptr<CommandPortImpl> port;
        RequestId id = 0;

    public:
        ~Reply();
        CROWY_DECLARE_NON_COPYABLE(Reply)
        Reply(Reply&&) noexcept;
        Reply& operator=(Reply&&) noexcept;

        void Ok(DOM::Value result={});
        void Error(Str message);

    private:
        friend class CommandPortImpl;

        Reply(std::weak_ptr<CommandPortImpl>, RequestId);

        void abandon() noexcept;
    };

    using VerbHandler = std::function<void(const DOM::Value&, Reply)>;
    // the argument keys a verb reads; the port refuses any other before the
    // handler runs
    using VerbKeys = std::vector<Str>;

    struct CommandPortConfig{
        // 0 binds an ephemeral port and skips the retries
        u16 port = 27500;
        u8 portRetries = 9;
        u32 maxConnections = 8;
        u32 maxBodyBytes = 1u << 20;
        f64 pendingTimeoutSeconds = 5.0;
        f64 idleTimeoutSeconds = 5.0;
    };

    struct CommandPortStatus{
        enum class Server: u8{
            Disabled,
            Listening,
            BindFailed
        };

        Server server = Server::Disabled;
        // the port actually bound
        u16 port = 0;
        bool everConnected = false;
        Str lastVerb;
        u64 drainCount = 0;
        // drainCount when lastVerb was dispatched
        u64 lastVerbDrain = 0;
        // open connections, the pending ones among them
        u32 connectionCount = 0;
        u32 pendingCount = 0;
        bool lastReplyFailed = false;
    };

    // An HTTP/1.1 subset on 127.0.0.1, pumped by Drain() once per frame on
    // the calling thread: accept, read, dispatch, reply, all inline.
    // POST /rpc {"cmd": "<verb>", "args": {...}} -> {"ok": true, "result": ...}
    // or {"ok": false, "error": "..."}; GET / lists the registered verbs and
    // the keys each takes.
    // The port itself registers only the reflection verbs - list_objects,
    // describe, get_property, set_property - generic over what Expose gave
    // it; every other verb is the owner's to register.
    class CommandPort{
    private:
        std::shared_ptr<CommandPortImpl> impl;

    public:
        ~CommandPort();
        CROWY_DECLARE_PINNED(CommandPort)

        explicit CommandPort(const CommandPortConfig& config = {});

        void RegisterVerb(Str name, VerbKeys keys, VerbHandler);
        // A name the reflection verbs address, resolved at every request.
        // The resolver must stay callable until the port is gone or the name
        // is unexposed; the callback fires after every successful write.
        void Expose(
            Str name,
            PropertyTarget target,
            DirtyCallback onDirty = {}
        );
        // an object whose address never changes
        void Expose(Str name, void* target, const TypeDesc&, DirtyCallback onDirty = {});
        void Unexpose(StrView name);
        void Drain();

        CommandPortStatus Status() const;
        // 0 while not listening
        u16 Port() const noexcept;
    };
}
