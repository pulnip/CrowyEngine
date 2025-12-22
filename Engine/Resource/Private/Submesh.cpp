#include "ResourceManager.hpp"
#include "Submesh.hpp"

namespace Crowy
{
    Submesh Submesh::make(const Request& request, LoadContext& ctx){
        // TODO
        throw std::runtime_error("Not implemented");
    }

    struct SubmeshManager::Impl{
        ResourceManager<Submesh> manager;
    };

    SubmeshManager::SubmeshManager()
        :impl(std::make_unique<Impl>()){}
    SubmeshManager::~SubmeshManager(){}

    SubmeshHandle SubmeshManager::getOrLoad(
        const Submesh::Request& request, LoadContext& ctx
    ){
        return impl->manager.getOrLoad(request, ctx);
    }

    Submesh* SubmeshManager::get(SubmeshHandle handle){
        return impl->manager.get(handle);
    }

    const Submesh* SubmeshManager::get(SubmeshHandle handle) const{
        return impl->manager.get(handle);
    }

    void SubmeshManager::unload(SubmeshHandle handle){
        impl->manager.unload(handle);
    }
}