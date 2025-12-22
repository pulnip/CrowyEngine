#include "Material.hpp"
#include "ResourceManager.hpp"

namespace Crowy
{
    Material Material::make(const Request& request, LoadContext& ctx){
        // TODO
        throw std::runtime_error("Not implemented");
    }

    struct MaterialManager::Impl{
        ResourceManager<Material> manager;
    };

    MaterialManager::MaterialManager()
        :impl(std::make_unique<Impl>()){}
    MaterialManager::~MaterialManager(){}

    MaterialHandle MaterialManager::getOrLoad(
        const Material::Request& request, LoadContext& ctx
    ){
        return impl->manager.getOrLoad(request, ctx);
    }

    Material* MaterialManager::get(MaterialHandle handle){
        return impl->manager.get(handle);
    }

    const Material* MaterialManager::get(MaterialHandle handle) const{
        return impl->manager.get(handle);
    }

    void MaterialManager::unload(MaterialHandle handle){
        impl->manager.unload(handle);
    }
}