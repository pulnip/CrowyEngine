#include <print>
#include <thread>
#include <utility>
#include "ClassRegistry.hpp"
#include "CommandPort.hpp"
#include "DOM.hpp"
#include "Object.hpp"

// The network HelloWorld: one port, two verbs, one exposed struct, a bare
// loop standing in for the frame. From another shell:
//   curl -s -X POST 127.0.0.1:27500/rpc -d '{"cmd":"ping"}'
//   curl -s 127.0.0.1:27500/
//   curl -s -X POST 127.0.0.1:27500/rpc -d '{"cmd":"describe","args":{"target":"knobs"}}'
//   curl -s -X POST 127.0.0.1:27500/rpc -d '{"cmd":"set_property","args":{"target":"knobs","path":"gain","value":2}}'
//   curl -s -X POST 127.0.0.1:27500/rpc -d '{"cmd":"quit"}'

enum class HelloBlend: Crowy::u8{
    Opaque,
    Additive
};

namespace Crowy
{
    CROWY_ENUM_BEGIN(HelloBlend)
        CROWY_ENUM_VALUE(Opaque)
        CROWY_ENUM_VALUE(Additive)
    CROWY_ENUM_END()
}

// the reflection verbs need nothing beyond a registered struct
struct HelloKnobs{
    Crowy::f32 gain = 1.0f;
    Crowy::Str label = "hello";
    HelloBlend blend = HelloBlend::Opaque;
};

CROWY_STRUCT(HelloKnobs)
    .SetProperty("gain", &HelloKnobs::gain)
        .SetUIRange(0.0f, 4.0f)
    .SetProperty("label", &HelloKnobs::label)
    .SetProperty("blend", &HelloKnobs::blend)
CROWY_STRUCT_END(HelloKnobs)

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
        HelloKnobs knobs;

        // the callback is the write's only side effect a frame would see
        port.Expose("knobs", &knobs, *GetDesc<HelloKnobs>(), [&knobs]{
            std::println("knobs: gain {} label {}", knobs.gain, knobs.label);
        });

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
