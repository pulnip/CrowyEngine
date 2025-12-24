#pragma once

#include "generic_handle.hpp"
#include "semantics.hpp"
#include "Resource.hpp"
#include "ResourceManager.hpp"

namespace Crowy
{
    Shader instantiate(const ShaderRequest&, LoadContext&);

    using ShaderHandle = generic_handle<Shader>;

    class ShaderManager{
    public:
        ShaderManager() = default;
        ~ShaderManager() = default;
        DECLARE_PINNED(ShaderManager)

        inline static auto singleton(){ return instance; }

        inline ShaderHandle getOrLoad(const Shader::Request& request, LoadContext& ctx){
            return manager.getOrLoad(request, ctx);
        }
        inline Shader* get(ShaderHandle handle){
            return manager.get(handle);
        }
        inline const Shader* get(ShaderHandle handle) const{
            return manager.get(handle);
        }
        void unload(ShaderHandle handle){
            manager.unload(handle);
        }

    private:
        static ShaderManager* instance;
        friend void initResourceModule(RHIDevice&);
        friend void deinitResourceModule();

        ResourceManager<Shader> manager;
    };
}