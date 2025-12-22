#include "ResourceManager.hpp"
#include "Shader.hpp"

namespace Crowy
{
    Shader Shader::make(const Request& request, LoadContext& ctx){
        // TODO
        throw std::runtime_error("Not implemented");
    }

    struct ShaderManager::Impl{
        ResourceManager<Shader> manager;
    };

    ShaderManager::ShaderManager()
        :impl(std::make_unique<Impl>()){}
    ShaderManager::~ShaderManager(){}

    ShaderHandle ShaderManager::getOrLoad(
        const Shader::Request& request, LoadContext& ctx
    ){
        return impl->manager.getOrLoad(request, ctx);
    }

    Shader* ShaderManager::get(ShaderHandle handle){
        return impl->manager.get(handle);
    }

    const Shader* ShaderManager::get(ShaderHandle handle) const{
        return impl->manager.get(handle);
    }

    void ShaderManager::unload(ShaderHandle handle){
        impl->manager.unload(handle);
    }
}