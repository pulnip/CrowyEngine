#pragma once

#include <memory>
#include <string>
#include <vector>
#include "generic_handle.hpp"
#include "semantics.hpp"
#include "Resource.hpp"
#include "ResourceManager.hpp"

namespace Crowy
{
    struct MaterialSetRequest{
        using Key     = std::string;
        using KeyHash = std::hash<std::string>;

        std::string meshKey;
        // Pre-loaded material handles
        // std::vector<MaterialHandle> materialHandles;

        inline Key key() const{ return meshKey; }
    };

    struct MaterialSet{
        using Request = MaterialSetRequest;

        std::vector<Material> materials;

        inline auto materialCount() const{
            return materials.size();
        }
        inline auto empty() const{
            return materials.empty();
        }
        inline auto isValid() const{
            return !materials.empty();
        }
    };

    MaterialSet instantiate(const MaterialSetRequest&, LoadContext&);

    using MaterialSetHandle = generic_handle<MaterialSet>;

    class MaterialSetManager{
    public:
        MaterialSetManager() = default;
        ~MaterialSetManager() = default;
        DECLARE_PINNED(MaterialSetManager)

        inline static auto singleton(){ return instance; }

        inline MaterialSetHandle getOrLoad(const MaterialSetRequest& request, LoadContext& ctx){
            return manager.getOrLoad(request, ctx);
        }
        inline MaterialSet* get(MaterialSetHandle handle){
            return manager.get(handle);
        }
        inline const MaterialSet* get(MaterialSetHandle handle) const{
            return manager.get(handle);
        }
        inline void unload(MaterialSetHandle handle){
            manager.unload(handle);
        }

    private:
        static MaterialSetManager* instance;
        friend void initResourceModule(RHIDevice&);
        friend void deinitResourceModule();

        ResourceManager<MaterialSet> manager;
    };
}