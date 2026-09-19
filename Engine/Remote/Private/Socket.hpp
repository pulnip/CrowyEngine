#pragma once

#include "Primitives.hpp"

// The one place the winsock / BSD split lives. Handles are opaque here so
// no socket header reaches the rest of the module; every call is
// non-blocking-aware and reports EWOULDBLOCK through wouldBlock().
namespace Crowy
{
    namespace Socket
    {
        using Handle = isize;

        inline constexpr Handle Invalid = -1;

        // winsock startup, once per process; a no-op elsewhere
        void initialize();

        Handle createTcp();
        void closeSocket(Handle);

        bool setNonBlocking(Handle);
        // SO_REUSEADDR on POSIX so a restart rebinds through TIME_WAIT;
        // SO_EXCLUSIVEADDRUSE on Windows, where reuse would allow hijacking
        bool setReuseAddress(Handle);
        // SO_NOSIGPIPE where it exists; a peer that hung up must not kill
        // the process on the next send
        bool setNoSigPipe(Handle);

        bool bindLoopback(Handle, u16 port);
        bool listenOn(Handle, int backlog);
        // Invalid when nothing is waiting (wouldBlock()) or on error
        Handle acceptOne(Handle);
        bool connectLoopback(Handle, u16 port);

        // > 0 bytes moved, 0 the peer closed, < 0 error (check wouldBlock())
        isize receiveSome(Handle, void* buffer, usize bytes);
        isize sendSome(Handle, const void* buffer, usize bytes);

        bool wouldBlock();
        int lastError();

        // the port actually bound, for a listener opened on port 0
        u16 boundPort(Handle);
    }
}
