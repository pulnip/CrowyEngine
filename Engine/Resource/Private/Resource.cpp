#include "LoadContext.hpp"
#include "MaterialSetManager.hpp"
#include "MeshManager.hpp"
#include "Resource.hpp"
#include "ShaderManager.hpp"

namespace Crowy
{
    static RHIDevice* device = nullptr;

    void initResourceModule(RHIDevice& device){
        Crowy::device = &device;

        MeshManager::instance        = new MeshManager();
        MaterialSetManager::instance = new MaterialSetManager();
        ShaderManager::instance      = new ShaderManager();
    }

    void deinitResourceModule(){
        delete ShaderManager::instance;
        delete MaterialSetManager::instance;
        delete MeshManager::instance;

        ShaderManager::instance      = nullptr;
        MaterialSetManager::instance = nullptr;
        MeshManager::instance        = nullptr;
    }

    MeshHandle getOrLoad(MeshRequest request){
        LoadContext context{
            .device = Crowy::device,
        };
        return MeshManager::singleton()->getOrLoad(request, context);
    }
    MaterialSetHandle getOrLoad(MaterialSetRequest request){
        LoadContext context{
            .device = Crowy::device,
        };
        return MaterialSetManager::singleton()->getOrLoad(request, context);
    }
    ShaderHandle getOrLoad(ShaderRequest request){
        LoadContext context{
            .device = Crowy::device,
        };
        return ShaderManager::singleton()->getOrLoad(request, context);
    }

    MeshView get(MeshHandle handle){
        auto mesh = MeshManager::singleton()->get(handle);

        return mesh->submeshes;
    }
    MaterialSetView get(MaterialSetHandle handle){
        auto materialSet = MaterialSetManager::singleton()->get(handle);

        return materialSet->materials;
    }
    Shader* get(ShaderHandle handle){
        return ShaderManager::singleton()->get(handle);
    }
}