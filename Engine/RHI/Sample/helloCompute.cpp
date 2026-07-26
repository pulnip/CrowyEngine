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

        auto pipelineState = device->CreatePipelineState(
            RHIComputePipelineStateDesc{
                .computeShader = {
                    .path = "Engine/Shader/HelloCompute.slang",
                    .entryPoint = "cs_main"
                }
            }
        );

        cmdList->Begin();

        cmdList->TransitionBarrier(*out,
            RHIResourceUsage::ComputeWrite
        );

        {
            cmdList->BeginCompute();

            cmdList->SetPipelineState(*pipelineState);
            cmdList->SetPushComputeConstants(PushConstants{
                .lhs = lhs->GetReadableID(sizeof(float)),
                .rhs = rhs->GetReadableID(sizeof(float)),
                .out = out->GetWritableID(sizeof(float))
            });

            cmdList->Dispatch({N, 1, 1});

            cmdList->EndCompute();
        }

        cmdList->TransitionBarrier(*out,
            RHIResourceUsage::CopySrc
        );

        {
            cmdList->BeginBlit();
            cmdList->Copy(
                *out,
                *readback,
                0,
                0,
                sizeof(float) * N
            );
            cmdList->EndBlit();
        }

        cmdList->Close();
        RHICommandList* cmdLists[] = {cmdList.get()};
        device->Submit(cmdLists, *fence);

        fence->WaitCPU(1);

        std::vector<float> result(N, 0.0f);
        readback->Download(result.data(), sizeof(float) * result.size());

        // verify result
        usize i = 0;
        for(; i<N; ++i){
            if(std::abs(result[i] - 2.0f) > 1e-3){
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
