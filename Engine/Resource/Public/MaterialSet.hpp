#pragma once

#include <memory>
#include <string>
#include <vector>
#include "generic_handle.hpp"
#include "math.hpp"
#include "Material.hpp"

namespace Crowy
{
    struct LoadContext;

    struct MaterialSet{
        std::vector<MaterialHandle> materials;

        inline auto materialCount() const{
            return materials.size();
        }
        inline auto empty() const{
            return materials.empty();
        }
        inline auto isValid() const{
            return !materials.empty();
        }

        struct Request{
            using Key     = std::string;
            using KeyHash = std::hash<std::string>;

            std::string meshKey;
            // Pre-loaded material handles
            std::vector<MaterialHandle> materialHandles;

            inline Key key() const{ return meshKey; }
        };

        static MaterialSet make(const Request&, LoadContext&);
    };
    using MaterialSetHandle = generic_handle<MaterialSet>;

    class MaterialSetManager{
    public:
        MaterialSetManager();
        ~MaterialSetManager();

        MaterialSetHandle getOrLoad(const MaterialSet::Request&,
            LoadContext& ctx);
        MaterialSet*       get(MaterialSetHandle);
        const MaterialSet* get(MaterialSetHandle) const;
        void unload(MaterialSetHandle);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}