#pragma once

#include <vector>

#include "CommandPort.hpp"
#include "FrameCaptures.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class FrameClock;

    using CommandPortPtr = RAII<CommandPort>;

    class FramePort {
    private:
        struct PendingWait {
            u64 frame = 0;
            Reply reply;
        };

        const FrameCaptures& captures;
        // null when disabled, inert with Status().server == BindFailed when
        // no port could bind
        CommandPortPtr port;
        // a wait the port already timed out stays until its frame ends, and
        // answering it then does nothing
        std::vector<PendingWait> pendingWaits;

    public:
        ~FramePort() = default;
        // the port's verbs point at it
        CROWY_DECLARE_PINNED(FramePort)

        explicit FramePort(const FrameCaptures& captures);

        // the port CROWY_COMMAND_PORT asks for, or none
        void Open();
        // on a port that bound
        void RegisterVerbs();
        // --hold, which throws without a bound port to release it
        void Hold();
        // --port-input, which throws without a bound port to push its input
        void CheckPortInput() const;
        // the waits whose frame ended, then every request that arrived
        void Drain();

        CommandPort* Port() noexcept { return port.get(); }

    private:
        FrameClock& clock() const;
        DOM::Table status() const;
        void answerWaits();
    };
}
