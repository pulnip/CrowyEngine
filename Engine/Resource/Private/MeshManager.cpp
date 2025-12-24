#include "LoadContext.hpp"
#include "MeshManager.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    MeshManager* MeshManager::instance = nullptr;

    Mesh instantiate(const MeshRequest& request, LoadContext& ctx){
    }
}