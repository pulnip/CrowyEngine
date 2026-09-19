#include "Socket.hpp"

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <cerrno>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace Crowy
{
    namespace Socket
    {
        namespace{
        #if defined(_WIN32)
            using Native = SOCKET;
            using Length = int;

            struct WinsockScope{
                WinsockScope(){
                    WSADATA data;
                    WSAStartup(MAKEWORD(2, 2), &data);
                }
                ~WinsockScope(){
                    WSACleanup();
                }
            };
        #else
            using Native = int;
            using Length = socklen_t;
        #endif

            Native native(Handle handle) noexcept{
                return static_cast<Native>(handle);
            }

            Handle handleOf(Native socket) noexcept{
            #if defined(_WIN32)
                return socket == INVALID_SOCKET ? Invalid : static_cast<Handle>(socket);
            #else
                return socket < 0 ? Invalid : static_cast<Handle>(socket);
            #endif
            }

            sockaddr_in loopback(u16 port) noexcept{
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_port = htons(port);
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

                return address;
            }
        }

        void initialize(){
        #if defined(_WIN32)
            static const WinsockScope scope;
        #endif
        }

        Handle createTcp(){
            return handleOf(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        }

        void closeSocket(Handle handle){
        #if defined(_WIN32)
            ::closesocket(native(handle));
        #else
            ::close(native(handle));
        #endif
        }

        bool setNonBlocking(Handle handle){
        #if defined(_WIN32)
            u_long mode = 1;

            return ::ioctlsocket(native(handle), FIONBIO, &mode) == 0;
        #else
            const int flags = ::fcntl(native(handle), F_GETFL, 0);
            if(flags < 0)
                return false;

            return ::fcntl(native(handle), F_SETFL, flags | O_NONBLOCK) == 0;
        #endif
        }

        bool setReuseAddress(Handle handle){
        #if defined(_WIN32)
            const BOOL exclusive = TRUE;

            return ::setsockopt(
                native(handle), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)
            ) == 0;
        #else
            const int reuse = 1;

            return ::setsockopt(
                native(handle), SOL_SOCKET, SO_REUSEADDR,
                &reuse, sizeof(reuse)
            ) == 0;
        #endif
        }

        bool setNoSigPipe(Handle handle){
        #if defined(SO_NOSIGPIPE)
            const int on = 1;

            return ::setsockopt(
                native(handle), SOL_SOCKET, SO_NOSIGPIPE,
                &on, sizeof(on)
            ) == 0;
        #else
            (void)handle;

            return true;
        #endif
        }

        bool bindLoopback(Handle handle, u16 port){
            const auto address = loopback(port);

            return ::bind(
                native(handle),
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)
            ) == 0;
        }

        bool listenOn(Handle handle, int backlog){
            return ::listen(native(handle), backlog) == 0;
        }

        Handle acceptOne(Handle handle){
            return handleOf(::accept(native(handle), nullptr, nullptr));
        }

        bool connectLoopback(Handle handle, u16 port){
            const auto address = loopback(port);

            return ::connect(
                native(handle),
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)
            ) == 0;
        }

        isize receiveSome(Handle handle, void* buffer, usize bytes){
            return ::recv(
                native(handle),
                static_cast<char*>(buffer),
                static_cast<Length>(bytes),
                0
            );
        }

        isize sendSome(Handle handle, const void* buffer, usize bytes){
        #if defined(MSG_NOSIGNAL)
            constexpr int flags = MSG_NOSIGNAL;
        #else
            constexpr int flags = 0;
        #endif

            return ::send(
                native(handle),
                static_cast<const char*>(buffer),
                static_cast<Length>(bytes),
                flags
            );
        }

        bool wouldBlock(){
        #if defined(_WIN32)
            return ::WSAGetLastError() == WSAEWOULDBLOCK;
        #else
            return errno == EAGAIN || errno == EWOULDBLOCK;
        #endif
        }

        int lastError(){
        #if defined(_WIN32)
            return ::WSAGetLastError();
        #else
            return errno;
        #endif
        }

        u16 boundPort(Handle handle){
            sockaddr_in address{};
            Length length = sizeof(address);
            if(::getsockname(
                native(handle),
                reinterpret_cast<sockaddr*>(&address),
                &length
            ) != 0){
                return 0;
            }

            return ntohs(address.sin_port);
        }
    }
}
