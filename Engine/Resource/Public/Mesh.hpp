#pragma once

#include <string>
#include <vector>
#include "generic_handle.hpp"
#include "Submesh.hpp"

namespace Crowy
{
    struct Mesh{
        std::vector<SubmeshHandle> submeshes;

        inline size_t submeshCount() const{
            return submeshes.size();
        }
        inline bool empty() const{
            return submeshes.empty();
        }
        inline bool isValid() const{
            return !submeshes.empty();
        }

        struct Request{
            using Key     = std::string;
            using KeyHash = std::hash<std::string>;

            std::string path;

            inline Key key() const{ return path; }
        };

        static Mesh make(const Request&, LoadContext&);
    };
    using MeshHandle = generic_handle<Mesh>;

    class MeshManager{
    public:
        MeshManager();
        ~MeshManager();

        MeshHandle getOrLoad(const Mesh::Request&, LoadContext&);
        Mesh*       get(MeshHandle);
        const Mesh* get(MeshHandle) const;
        void unload(MeshHandle);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}