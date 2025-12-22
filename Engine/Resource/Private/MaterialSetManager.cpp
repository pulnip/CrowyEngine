#include "LoadContext.hpp"
#include "MaterialSetManager.hpp"

namespace Crowy
{
    MaterialSetManager* MaterialSetManager::instance = nullptr;

    MaterialSet instantiate(const MaterialSetRequest& request, LoadContext& ctx){
        // TODO
        throw std::runtime_error("Not implemented");
    }
}