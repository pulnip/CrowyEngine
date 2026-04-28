#pragma once

#include <string>
#include <unordered_map>
#include "generic_handle.hpp"
#include "semantics.hpp"
#include "ModelData.hpp"
#include "Resource.hpp"
#include "ResourceManager.hpp"

namespace Crowy
{
    struct MaterialSetRequest{
        using Key     = std::string;
        using KeyHash = std::hash<Key>;

        std::string uri;
        std::unordered_map<std::string, MaterialRef> data;

        inline Key key() const{ return uri; }
    };

    using MaterialMap = std::unordered_map<std::string, Material>;

    struct MaterialSet{
        using Request = MaterialSetRequest;

        MaterialMap materials;

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
        CROWY_DECLARE_PINNED(MaterialSetManager)

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
        friend void initResourceModule(RHIDevice*);
        friend void deinitResourceModule();

        ResourceManager<MaterialSet> manager;
    };

    #define MaterialSetManager_ MaterialSetManager::singleton()
}