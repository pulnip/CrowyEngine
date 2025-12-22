#include "MaterialSet.hpp"
#include "ResourceManager.hpp"

namespace Crowy
{
    MaterialSet MaterialSet::make(const Request& request, LoadContext& ctx){
        // TODO
        throw std::runtime_error("Not implemented");
    }

    struct MaterialSetManager::Impl{
        ResourceManager<MaterialSet> manager;
    };

    MaterialSetManager::MaterialSetManager()
        :impl(std::make_unique<Impl>()){}
    MaterialSetManager::~MaterialSetManager(){}

    MaterialSetHandle MaterialSetManager::getOrLoad(
        const MaterialSet::Request& request, LoadContext& ctx
    ){
        return impl->manager.getOrLoad(request, ctx);
    }

    MaterialSet* MaterialSetManager::get(MaterialSetHandle handle){
        return impl->manager.get(handle);
    }

    const MaterialSet* MaterialSetManager::get(MaterialSetHandle handle) const{
        return impl->manager.get(handle);
    }

    void MaterialSetManager::unload(MaterialSetHandle handle){
        impl->manager.unload(handle);
    }
}