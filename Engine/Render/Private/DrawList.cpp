#include "DrawList.hpp"

#include <algorithm>
#include <iterator>
#include <span>

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    DrawList::~DrawList() = default;

    DrawList::DrawList(RHIDevice& device, u32 drawReserve)
        : device(device) {
        rows.reserve(drawReserve);
        args.reserve(drawReserve);
        sorted.reserve(drawReserve);
    }

    void DrawList::Build(
        const RenderScene& scene,
        const VisibleSet& visible,
        PipelineCache& cache,
        const PassPipelineDesc& pass,
        const DrawFilter& filter,
        DrawOrder order
    ) {
        const auto& materials = scene.Materials();
        const auto materialCount = materials.Count();

        pipelineOfMaterial.assign(materialCount, nullptr);
        runOfMaterial.assign(materialCount, 0);
        pipelineOrder.clear();
        for(usize i = 0; i < materialCount; ++i) {
            const auto& material = materials.At(i).pipeline;
            CROWY_ASSERT(
                std::has_single_bit(static_cast<u32>(material.domain)),
                "a material belongs to exactly one domain"
            );
            if(!hasFlag(filter.domains, material.domain))
                continue;

            auto* pso = &cache.Resolve(material, pass);
            const auto found = std::ranges::find(pipelineOrder, pso);
            runOfMaterial[i] =
                static_cast<u32>(std::distance(pipelineOrder.begin(), found));
            if(found == pipelineOrder.end())
                pipelineOrder.push_back(pso);
            pipelineOfMaterial[i] = pso;
        }
        CROWY_ASSERT(
            pipelineOrder.size() <= 0x1'0000,
            "a list keys at most 65536 pipelines"
        );

        sorted.clear();
        triangleCount = 0;
        for(u32 i = 0; i < visible.draws.size(); ++i) {
            const auto& draw = visible.draws[i];
            if(pipelineOfMaterial[draw.materialIndex] == nullptr)
                continue;
            if(!hasAll(draw.flags, filter.required))
                continue;

            const auto topology =
                materials.At(draw.materialIndex).pipeline.topology;
            CROWY_ASSERT(
                topology != RHIPrimitiveTopology::TriangleStrip,
                "a strip material needs its own triangle count"
            );
            if(topology == RHIPrimitiveTopology::TriangleList)
                triangleCount += draw.geometry.indexCount / 3;

            const auto key = order == DrawOrder::PipelineThenNearFirst ?
                opaqueSortKey(
                    runOfMaterial[draw.materialIndex],
                    draw.depth,
                    draw.identity
                ) :
                translucentSortKey(draw.depth, draw.identity);
            sorted.emplace_back(key, i);
        }
        std::ranges::sort(sorted);

        const auto primitives = scene.Primitives().All();
        rows.clear();
        args.clear();
        runs.clear();
        for(const auto& [key, index]: sorted) {
            const auto& draw = visible.draws[index];
            auto* pso = pipelineOfMaterial[draw.materialIndex];
            const auto slot = static_cast<u32>(rows.size());

            if(runs.empty() || runs.back().pso != pso)
                runs.push_back(DrawRun{.pso = pso, .firstDraw = slot});
            ++runs.back().drawCount;

            rows.push_back(
                DrawData{
                    .world = primitives[draw.primitive].localToWorld,
                    .materialIndex = draw.materialIndex,
                    .objectID = draw.primitive,
                    .vbIndex = static_cast<u32>(draw.geometry.baseVertex)
                }
            );
            args.push_back(
                RHIDrawIndexedArgs{
                    .indexCount = draw.geometry.indexCount,
                    .firstIndex = draw.geometry.firstIndex,
                    // the pool offset rides in vbIndex instead, because
                    // SV_VertexID picks this up on Metal but not on D3D12
                    .baseVertex = 0,
                    // the row index inside this list's slice
                    .baseInstance = slot
                }
            );
        }

        uploaded = false;
    }

    void DrawList::Upload() {
        if(!rows.empty()) {
            rowSlice = device.UploadTransient(
                std::span<const DrawData>(rows),
                static_cast<u32>(sizeof(DrawData))
            );
            argsSlice = device.UploadTransient(
                std::span<const RHIDrawIndexedArgs>(args),
                static_cast<u32>(sizeof(RHIDrawIndexedArgs))
            );
        }

        uploaded = true;
    }

    ScenePush DrawList::Push(ScenePush frame) const {
        CROWY_ASSERT(
            uploaded,
            "Push() before Upload(): this list's rows have no slice yet"
        );

        constexpr auto drawStride = static_cast<u32>(sizeof(DrawData));

        // one descriptor spans the whole transient buffer; the base index is
        // what points the shader at this list's rows inside it
        frame.draws = rows.empty() ?
            0 :
            rowSlice.buffer->GetReadableID(drawStride);
        frame.drawBase = rows.empty() ? 0 : rowSlice.offset / drawStride;

        return frame;
    }

    void DrawList::Submit(
        RHICommandList& cmdList,
        const RHIIndexBufferView& indices
    ) const {
        CROWY_ASSERT(
            uploaded,
            "Submit() before Upload(): this list's args have no slice yet"
        );

        for(const auto& run: runs) {
            cmdList.ExecuteIndirectIndexed(
                DrawBatchIndexed{
                    .pso = run.pso,
                    .args = argsSlice.buffer,
                    .argsOffset = argsSlice.offset +
                        run.firstDraw * sizeof(RHIDrawIndexedArgs),
                    .drawCount = run.drawCount,
                    .indices = indices
                }
            );
        }
    }
}
