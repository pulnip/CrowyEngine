#include "LoadContext.hpp"
#include "MeshManager.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    MeshManager* MeshManager::instance = nullptr;

    Mesh instantiate(const MeshRequest& request, LoadContext& ctx){
        Mesh mesh;

        for(const auto& submeshData: request.data){
            auto vbuffer = ctx.device->createBuffer(
                RHIBufferCreateDesc{
                    .size = submeshData.vertices.size() * sizeof(Vertex),
                    .usage = RHIBufferUsage::VertexBuffer,
                    .stride = 0,
                    .initialData = submeshData.vertices.data()
                }
            );
            auto ibuffer = ctx.device->createBuffer(
                RHIBufferCreateDesc{
                    .size = submeshData.indices.size() * sizeof(uint32_t),
                    .usage = RHIBufferUsage::IndexBuffer,
                    .stride = 0,
                    .initialData = submeshData.indices.data()
                }
            );
            mesh.submeshes.push_back(Submesh{
                .vertexBuffer = std::move(vbuffer),
                .indexBuffer = std::move(ibuffer),
                .vertexCount = submeshData.vertexCount(),
                .indexCount = submeshData.indexCount(),
                .vertexStride = 0,
                .materialSlotName = submeshData.materialSlotName
            });
        }

        return mesh;
    }
}