#include "Mesh.hpp"
#include "ResourceManager.hpp"

namespace Crowy
{
    Mesh Mesh::make(const Request& request, LoadContext& ctx){
        // TODO
        throw std::runtime_error("Not implemented");
    }

    struct MeshManager::Impl{
        ResourceManager<Mesh> manager;
    };

    MeshManager::MeshManager()
        :impl(std::unique_ptr<Impl>()){}
    MeshManager::~MeshManager(){}

    MeshHandle MeshManager::getOrLoad(
        const Mesh::Request& request, LoadContext& ctx
    ){
        return impl->manager.getOrLoad(request, ctx);
    }

    Mesh* MeshManager::get(MeshHandle handle){
        return impl->manager.get(handle);
    }

    const Mesh* MeshManager::get(MeshHandle handle) const{
        return impl->manager.get(handle);
    }

    void MeshManager::unload(MeshHandle handle){
        impl->manager.unload(handle);
    }
}