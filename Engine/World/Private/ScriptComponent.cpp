#include "ScriptComponent.hpp"

#include "Component.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        // a row outlives its actor until Sync; the actor's script does not
        void runScripts(World& world, const FrameTime& time) {
            world.Each<ScriptComponent>([&](ActorId actor,
                                            const ScriptComponent& script) {
                if(world.IsValid(actor))
                    script.run(world, actor, time);
            });
        }
    }

    // clang-format off
    CROWY_COMPONENT(ScriptComponent)
        .Transient()
    CROWY_COMPONENT_END(ScriptComponent)

    CROWY_SYSTEM(RunScripts, Update, &runScripts)
    CROWY_SYSTEM_END(RunScripts)
    // clang-format on
}
