#include <print>
#include <thread>
#include <utility>
#include "CommandPort.hpp"
#include "DOM.hpp"

// The network HelloWorld: one port, two verbs, a bare loop standing in for
// the frame. From another shell:
//   curl -s -X POST 127.0.0.1:27500/rpc -d '{"cmd":"ping"}'
//   curl -s 127.0.0.1:27500/
//   curl -s -X POST 127.0.0.1:27500/rpc -d '{"cmd":"quit"}'

int main(void){
    using namespace Crowy;
    using namespace std::chrono_literals;

    try{
        CommandPort port;
        if(port.Port() == 0){
            std::println("no port could be bound");

            return 1;
        }

        bool quitRequested = false;

        port.RegisterVerb("ping", [&port](auto&, Reply reply){
            DOM::Table result;
            result.emplace("pong", true);
            result.emplace("drain", static_cast<i64>(port.Status().drainCount));

            reply.Ok(std::move(result));
        });
        port.RegisterVerb("quit", [&quitRequested](auto&, Reply reply){
            quitRequested = true;

            reply.Ok();
        });

        std::println("listening on 127.0.0.1:{}", port.Port());

        // a frame, without the rest of one
        while(!quitRequested){
            port.Drain();
            std::this_thread::sleep_for(16ms);
        }

        std::println("Succeed!");
    }
    catch(const std::exception& e){
        std::println("Exception: {}", e.what());

        return 1;
    }

    return 0;
}
