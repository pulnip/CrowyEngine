#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "CommandPort.hpp"
#include "FrameCaptures.hpp"
#include "FrameControl.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    using CommandPortPtr = RAII<CommandPort>;

    // The loop the port's control verbs drive; App's for a RenderApp.
    struct FrameLoop {
        FrameControl& control;
        // the last frame that ended
        std::function<u64()> frame;
        std::function<f64()> elapsedSeconds;
        std::function<void()> requestQuit;
        // what ping names the app
        Str title;
    };

    // The command port and the verbs that drive the loop: ping, quit, run,
    // hold, step and wait_frame. Every control verb answers with where the
    // loop stands afterwards.
    class FramePort {
    private:
        // a wait_frame reply, held until its frame has ended
        struct PendingWait {
            u64 frame = 0;
            Reply reply;
        };

        FrameLoop loop;
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

        FramePort(FrameLoop loop, const FrameCaptures& captures);

        // the port CROWY_COMMAND_PORT asks for, or none
        void Open();
        // on a port that bound
        void RegisterVerbs();
        // --hold, which throws without a bound port to release it
        void Hold();
        // the waits whose frame ended, then every request that arrived
        void Drain();

        CommandPort* Port() noexcept { return port.get(); }

    private:
        DOM::Table status() const;
        void answerWaits();
    };
}
