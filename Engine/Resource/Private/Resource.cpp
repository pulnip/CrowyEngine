#include "LoadContext.hpp"
#include "MaterialSetManager.hpp"
#include "MeshManager.hpp"
#include "ModelImporter.hpp"
#include "Resource.hpp"
#include "RHIBuffer.hpp"
#include "RHIShader.hpp"
#include "RHITexture.hpp"
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

    std::pair<MeshHandle, MaterialSetHandle> getOrLoad(ModelRequest request){
        LoadContext context{
            .device = Crowy::device,
        };

        auto modelData = importModel(request.uri, Crowy::device->getCapabilities());

        if(!modelData.has_value()){
            return {MeshHandle::invalidHandle(), MaterialSetHandle::invalidHandle()};
        }

        auto meshHandle = MeshManager::singleton()->getOrLoad(
            MeshRequest{
                .uri = request.uri,
                .data = modelData->submeshes
            }, context
        );
        auto materialSetHandle = MaterialSetManager::singleton()->getOrLoad(
            MaterialSetRequest{
                .uri = request.uri,
                .data = std::move(modelData->materials)
            }, context
        );
        return {meshHandle, materialSetHandle};
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
        MaterialSetView view;

        for(const auto& [slot, material]: materialSet->materials){
            view.emplace(slot, &material);
        }

        return view;
    }
    Shader* get(ShaderHandle handle){
        return ShaderManager::singleton()->get(handle);
    }
}