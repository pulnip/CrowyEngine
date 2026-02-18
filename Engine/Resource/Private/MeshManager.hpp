#pragma once

#include <string>
#include <vector>
#include "generic_handle.hpp"
#include "semantics.hpp"
#include "ModelData.hpp"
#include "Resource.hpp"
#include "ResourceManager.hpp"
#include "RHIBuffer.hpp"

namespace Crowy
{
    struct MeshRequest{
        using Key     = std::string;
        using KeyHash = std::hash<Key>;

        std::string uri;
        std::span<const SubmeshData> data;

        inline Key key() const{ return uri; }
    };

    struct Mesh{
        using Request = MeshRequest;

        std::vector<Submesh> submeshes;

        inline size_t submeshCount() const{
            return submeshes.size();
        }
        inline bool empty() const{
            return submeshes.empty();
        }
        inline bool isValid() const{
            return !submeshes.empty();
        }
    };

    Mesh instantiate(const MeshRequest&, LoadContext&);

    using MeshHandle = generic_handle<Mesh>;

    class MeshManager{
    public:
        MeshManager() = default;
        ~MeshManager() = default;
        CROWY_DECLARE_PINNED(MeshManager)

        inline static auto singleton(){ return instance; }

        inline MeshHandle getOrLoad(const MeshRequest& request, LoadContext& ctx){
            return manager.getOrLoad(request, ctx);
        }
        inline Mesh* get(MeshHandle handle){
            return manager.get(handle);
        }
        inline const Mesh* get(MeshHandle handle) const{
            return manager.get(handle);
        }
        inline void unload(MeshHandle handle){
            manager.unload(handle);
        }

    private:
        static MeshManager* instance;
        friend void initResourceModule(RHIDevice*);
        friend void deinitResourceModule();

        ResourceManager<Mesh> manager;
    };

    #define MeshManager_ MeshManager::singleton()
}