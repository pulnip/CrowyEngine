#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"

using namespace Crowy;

constexpr uint32_t nextPow2(uint32_t n){
    return 1 << (32 - __builtin_clz(n-1));
}

int main(int argc, char* argv[]){
    auto device = createDevice();
    auto cmdList = device->createCommandList();

    constexpr auto N = 2000;
    constexpr auto N2 = nextPow2(N);
    std::vector<uint32_t> ones(N2, 1);

    Renderer renderer(device.get());
    RenderSpec spec{
        .buffers = {
            {
                "BufferIn",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * N2,
                    .usage = combine(
                        RHIBufferUsage::CPUWrite,
                        RHIBufferUsage::ShaderResource
                    ),
                    .stride = 0,
                    .initialData = ones.data()
                }
            },
            {
                "BufferOut",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * N2,
                    .usage = combine(
                        RHIBufferUsage::CPURead,
                        RHIBufferUsage::UnorderedAccess
                    ),
                    .stride = 0,
                    .initialData = nullptr
                }
            }
        },
        .computePasses = {
            {
                .name = "PrefixSum",
                .inputBuffers = {
                    {
                        .name = "BufferIn",
                        .slot = 0
                    }
                },
                .outputBuffers = {
                    {
                        .name = "BufferOut",
                        .slot = 1
                    }
                },
                .shader = {
                #ifdef CROWY_METALRHI
                    .filePath = "asset/Shaders/prefix_sum.metal",
                    .funcName = "cs_prefix_sum",
                #elif CROWY_D3DRHI
                    .filePath = L"asset/Shaders/prefix_sum.hlsl",
                    .funcName = "cs_prefix_sum",
                #endif
                },
                .gridSize = {N2/2, 1, 1},
                .threadGroupSize = RHISize3D{N2/2, 1, 1}
            }
        }
    };

    try{
        renderer.loadPasses(spec);
    }
    catch(const std::exception& e){
        std::println("{}", e.what());

        return 0;
    }

    cmdList->begin();
    renderer.render(*cmdList);
    cmdList->close();

    device->submit(*cmdList);
    cmdList->waitUntilCompleted();

    auto outBuf = renderer.getBuffer("BufferOut");
    std::vector<uint32_t> result(N, 0);
    outBuf->download(result.data(), sizeof(float) * result.size());

    for(size_t i=0; i<N; ++i){
        if(result[i] != i){
            std::println("wrong result after [{}] = {}", i, result[i]);
            return 0;
        }
    }

    std::println("Success!");
    return 0;
}