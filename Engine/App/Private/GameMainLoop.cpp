#include "ECSSystem.hpp"
#include "GameMainLoop.hpp"
#include "Resource.hpp"

namespace Crowy
{
    void attachDefaultECSSystems(ECSScheduler& scheduler){
        scheduler.attach(std::make_unique<RenderSystem>());
    }

    void GameMainLoop::initialize(){
        attachDefaultECSSystems(scheduler);
    }

    bool GameMainLoop::update(float dt){
        UpdateContext context{
            .dt = 1.0f / 60
        };
        scheduler.update(registry, context);

        return true;
    }

    void GameMainLoop::finalize(){

    }
}