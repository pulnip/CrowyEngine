#include "LoadContext.hpp"
#include "Log.hpp"
#include "MaterialSetManager.hpp"
#include "MeshManager.hpp"
#include "ModelImporter.hpp"
#include "Resource.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    static RHIDevice* device = nullptr;

    void initResourceModule(RHIDevice* device){
        Crowy::device = device;

        MeshManager::instance        = new MeshManager();
        MaterialSetManager::instance = new MaterialSetManager();
    }

    void deinitResourceModule(){
        delete MaterialSetManager::instance;
        delete MeshManager::instance;

        MaterialSetManager::instance = nullptr;
        MeshManager::instance        = nullptr;

        Crowy::device = device;
    }

    std::pair<MeshHandle, MaterialSetHandle> getOrLoad(const RenderObjectSpec& spec){
        if(!Crowy::device){
            LOG_ERROR(LOG_RESOURCE, "Resource module not initialized");
            return {
                MeshHandle::invalidHandle(),
                MaterialSetHandle::invalidHandle()
            };
        }

        LoadContext context{
            .device = Crowy::device,
        };

        auto modelData = importModel(spec.uri, Crowy::device->getCapabilities());

        if(!modelData.has_value()){
            return {MeshHandle::invalidHandle(), MaterialSetHandle::invalidHandle()};
        }

        auto meshHandle = MeshManager_->getOrLoad(
            MeshRequest{
                .uri = spec.uri,
                .data = modelData->submeshes
            }, context
        );

        for(const auto& spec: spec.material_override){
            const auto& baseColor = spec.baseColor;
            const auto& targetSlot = spec.targetSlot;

            modelData->materials[targetSlot] = MaterialRef{
                .textures = {
                    {TextureSemantic::BaseColor, TextureRef{.path = baseColor}}
                }
            };
        }

        auto materialSetHandle = MaterialSetManager_->getOrLoad(
            MaterialSetRequest{
                .uri = spec.uri,
                .data = std::move(modelData->materials)
            }, context
        );
        return {meshHandle, materialSetHandle};
    }

    MeshView get(MeshHandle handle){
        auto mesh = MeshManager_->get(handle);

        return mesh->submeshes;
    }
    MaterialSetView get(MaterialSetHandle handle){
        auto materialSet = MaterialSetManager_->get(handle);
        MaterialSetView view;

        for(const auto& [slot, material]: materialSet->materials){
            view.emplace(slot, &material);
        }

        return view;
    }
}