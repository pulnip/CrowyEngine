#include "LoadContext.hpp"
#include "MeshManager.hpp"

namespace Crowy
{
    MeshManager* MeshManager::instance = nullptr;

    Mesh instantiate(const MeshRequest& request, LoadContext& ctx){
        // TODO
        throw std::runtime_error("Not implemented");
    }
}