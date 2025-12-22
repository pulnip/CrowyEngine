#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "generic_handle.hpp"
#include "RHIDefinitions.h"

namespace Crowy
{
    struct LoadContext;

    struct Submesh{
        RHIBufferHandle vertexBuffer;
        RHIBufferHandle indexBuffer;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t vertexStride = 0;

        inline bool isValid() const{
            return vertexBuffer.isValid() && vertexCount > 0;
        }

        inline bool hasIndices() const{
            return indexBuffer.isValid() && indexCount > 0;
        }

        struct Request{
            using Key     = std::string;
            using KeyHash = std::hash<std::string>;

            std::string path;

            inline Key key() const{ return path; }
        };

        static Submesh make(const Request&, LoadContext&);
    };
    using SubmeshHandle = generic_handle<Submesh>;

    class SubmeshManager{
    public:
        SubmeshManager();
        ~SubmeshManager();

        SubmeshHandle getOrLoad(const Submesh::Request&,
            LoadContext& ctx);
        Submesh*       get(SubmeshHandle);
        const Submesh* get(SubmeshHandle) const;
        void unload(SubmeshHandle);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}