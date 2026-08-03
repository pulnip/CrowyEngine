#include <array>
#include <print>
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"
#include "RHIPipelineState.hpp"

namespace{
    struct PushConstants{
        Crowy::u64 lhs;
        Crowy::u64 rhs;
        Crowy::u64 out;
    };
}

int main(void){
    try{
        using namespace Crowy;

        auto device = CreateDevice();
        auto cmdList = device->CreateCommandList();
        auto fence = device->CreateFence();

        constexpr size_t N = 1 << 20;
        std::vector<float> floats(N, 1.0f);
        RHIBufferCreateDesc desc{
            .size = sizeof(float) * N,
            .usage = RHIBufferUsage::None,
            .access = RHIMemoryAccess::GPUOnly,
            .initialData = floats.data()
        };
        auto lhs = device->CreateBuffer(desc, "LHS");
        auto rhs = device->CreateBuffer(desc, "RHS");

        auto out = device->CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(float) * N,
            .usage = RHIBufferUsage::UnorderedAccess,
            .access = RHIMemoryAccess::GPUOnly,
            .initialData = nullptr
        }, "OUT");
        auto readback = device->CreateBuffer(RHIBufferCreateDesc{
            .size = sizeof(float) * N,
            .usage = RHIBufferUsage::CopyDst,
            .access = RHIMemoryAccess::CPURead,
            .initialData = nullptr
        }, "ReadBack");

        auto addPipeline = device->CreatePipelineState(
            RHIComputePipelineStateDesc{
                .computeShader = {
                    .path = "Engine/Shader/HelloCompute.slang",
                    .entryPoint = "cs_main"
                }
            }
        );
        auto doublePipeline = device->CreatePipelineState(
            RHIComputePipelineStateDesc{
                .computeShader = {
                    .path = "Engine/Shader/HelloCompute.slang",
                    .entryPoint = "cs_double"
                }
            }
        );

        cmdList->Begin();

        // the compute pass releases `out`, the copy pass acquires it -
        // one edge, same value on both ends
        const auto outEdge = MakeBarrier(*out,
            RHIResourceUsage::StorageCompute,
            RHIResourceUsage::CopySrc
        );

        {
            const std::array acquires{
                // first GPU use of `out`: self-contained acquire
                MakeBarrier(*out,
                    RHIResourceUsage::Undefined,
                    RHIResourceUsage::StorageCompute
                )
            };
            cmdList->BeginComputePass({}, acquires);

            const PushConstants pushConstants{
                .lhs = lhs->GetReadableID(sizeof(float)),
                .rhs = rhs->GetReadableID(sizeof(float)),
                .out = out->GetWritableID(sizeof(float))
            };

            cmdList->SetPipelineState(*addPipeline);
            cmdList->SetPushComputeConstants(pushConstants);
            cmdList->Dispatch({N, 1, 1});

            // the second dispatch reads what the first just wrote —
            // a UAV → UAV hazard inside one pass, DispatchBarrier territory
            const std::array hazards{
                MakeBarrier(*out,
                    RHIResourceUsage::StorageCompute,
                    RHIResourceUsage::StorageCompute
                )
            };
            cmdList->DispatchBarrier({}, hazards);

            cmdList->SetPipelineState(*doublePipeline);
            cmdList->SetPushComputeConstants(pushConstants);
            cmdList->Dispatch({N, 1, 1});

            const std::array releases{outEdge};
            cmdList->EndComputePass({}, releases);
        }

        {
            // the readback buffer needs no acquire: readback-heap resources
            // never transition, and this copy is its first GPU use
            const std::array acquires{outEdge};
            cmdList->BeginBlitPass({}, acquires);
            cmdList->Copy(
                *out,
                *readback,
                0,
                0,
                sizeof(float) * N
            );
            // the CPU readback after the fence needs no release -
            // that ordering is the fence's job
            cmdList->EndBlitPass();
        }

        cmdList->Close();
        RHICommandList* cmdLists[] = {cmdList.get()};
        device->Submit(cmdLists, *fence);

        fence->WaitCPU(1);

        // forced push for resolve the in-flight state
        device->GetFrameIndexRef() += RHI_FRAMES_IN_FLIGHT - 1;

        std::vector<float> result(N, 0.0f);
        readback->Download(result.data(), sizeof(float) * result.size());

        // verify result: (1 + 1) doubled by the chained dispatch
        usize i = 0;
        for(; i<N; ++i){
            if(std::abs(result[i] - 4.0f) > 1e-3){
                std::println("wrong result: out[{}] = {}", i, result[i]);
                break;
            }
        }

        if(i == N){
            std::println("Succeed!");
        }
    }
    catch(const std::exception& e){
        std::println("Exception: {}", e.what());

        return 1;
    }

    return 0;
}
