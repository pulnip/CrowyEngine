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

    class Reply {
    private:
        std::weak_ptr<CommandPortImpl> port;
        RequestId id = 0;

    public:
        ~Reply();
        CROWY_DECLARE_NON_COPYABLE(Reply)
        Reply(Reply&&) noexcept;
        Reply& operator=(Reply&&) noexcept;

        void Ok(DOM::Value result = {});
        void Error(Str message);

    private:
        friend class CommandPortImpl;

        Reply(std::weak_ptr<CommandPortImpl>, RequestId);

        void abandon() noexcept;
    };

    using VerbHandler = std::function<void(const DOM::Value&, Reply)>;
    using VerbKeys = std::vector<Str>;

    struct CommandPortConfig {
        u16 port = 27500;
        u8 portRetries = 9;
        u32 maxConnections = 8;
        u32 maxBodyBytes = 1u << 20;
        f64 pendingTimeoutSeconds = 5.0;
        f64 idleTimeoutSeconds = 5.0;
    };

    struct CommandPortStatus {
        enum class Server : u8 { Disabled, Listening, BindFailed };

        Server server = Server::Disabled;
        u16 port = 0;
        bool everConnected = false;
        Str lastVerb;
        u64 drainCount = 0;
        u64 lastVerbDrain = 0;
        u32 connectionCount = 0;
        u32 pendingCount = 0;
        bool lastReplyFailed = false;
    };

    class CommandPort {
    private:
        std::shared_ptr<CommandPortImpl> impl;

    public:
        ~CommandPort();
        CROWY_DECLARE_PINNED(CommandPort)

        explicit CommandPort(const CommandPortConfig& config = {});

        void RegisterVerb(Str name, VerbKeys keys, VerbHandler);
        // The resolver must stay callable until the port is gone or the name
        // is unexposed.
        void Expose(
            Str name,
            PropertyTarget target,
            DirtyCallback onDirty = {}
        );
        // an object whose address never changes
        void Expose(
            Str name,
            void* target,
            const TypeDesc&,
            DirtyCallback onDirty = {}
        );
        void Unexpose(StrView name);
        void Drain();

        CommandPortStatus Status() const;
        u16 Port() const noexcept;
    };
}
