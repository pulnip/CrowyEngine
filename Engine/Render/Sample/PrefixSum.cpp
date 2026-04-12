#include <bit>
#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"

using namespace Crowy;

constexpr auto nextPow2(uint32_t n){
    return 1 << (32 - std::countl_zero(n-1));
}

int main(int argc, char* argv[]){
    auto device = createDevice();
    auto cmdList = device->createCommandList();

    constexpr auto N = 10000;
    constexpr auto GROUP_SIZE = 1024;
    constexpr auto NUM_GROUP = (N/2)/GROUP_SIZE + 1;
    constexpr auto N2 = 2 * NUM_GROUP * GROUP_SIZE;
    std::vector<uint32_t> ones(N2, 1);
    typeof(ones) c;
    decltype(ones)::value_type v;

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
                    )
                }
            },
            {
                "GroupSums",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * nextPow2(NUM_GROUP),
                    .usage = combine(
                        RHIBufferUsage::ShaderResource,
                        RHIBufferUsage::UnorderedAccess,

                        RHIBufferUsage::CPURead
                    )
                }
            }
        },
        .computePasses = {
            {
                .name = "Local PrefixSum",
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
                    },
                    {
                        .name = "GroupSums",
                        .slot = 2
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
                .gridSize = {.x=N2/2, .y=1, .z=1},
                .threadGroupSize = RHISize3D{
                    .x=std::min(N2/2, 1024),
                    .y=1,
                    .z=1
                }
            },
            {
                .name = "Group PrefixSum",
                .inputBuffers = {
                    {
                        .name = "GroupSums",
                        .slot = 0
                    }
                },
                .outputBuffers = {
                    {
                        .name = "GroupSums",
                        .slot = 1
                    },
                },
                .shader = {
                #ifdef CROWY_METALRHI
                    .filePath = "asset/Shaders/prefix_sum.metal",
                    .funcName = "cs_prefix_sum_single",
                #elif CROWY_D3DRHI
                    .filePath = L"asset/Shaders/prefix_sum.hlsl",
                    .funcName = "cs_prefix_sum_single",
                #endif
                },
                .gridSize = {.x=nextPow2(NUM_GROUP)/2, .y=1, .z=1},
                .threadGroupSize = RHISize3D{
                    .x=std::min(nextPow2(NUM_GROUP)/2, 1024),
                    .y=1,
                    .z=1
                }
            },
            {
                .name = "PrefixSum",
                .inputBuffers = {
                    {
                        .name = "GroupSums",
                        .slot = 0
                    }
                },
                .outputBuffers = {
                    {
                        .name = "BufferOut",
                        .slot = 1
                    },
                },
                .shader = {
                #ifdef CROWY_METALRHI
                    .filePath = "asset/Shaders/prefix_sum.metal",
                    .funcName = "cs_add_group_sums",
                #elif CROWY_D3DRHI
                    .filePath = L"asset/Shaders/prefix_sum.hlsl",
                    .funcName = "cs_add_group_sums",
                #endif
                },
                .gridSize = {.x=N2/2, .y=1, .z=1},
                .threadGroupSize = RHISize3D{
                    .x=std::min(N2/2, 1024),
                    .y=1,
                    .z=1
                }
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
    outBuf->download(result.data(), sizeof(uint32_t) * result.size());

    auto outBuf2 = renderer.getBuffer("GroupSums");
    std::vector<uint32_t> result2(NUM_GROUP, 0);
    outBuf2->download(result2.data(), sizeof(uint32_t) * result2.size());

    for(size_t i=0; i<N; ++i){
        if(result[i] != i){
            std::println("wrong result after [{}] = {}", i, result[i]);
            return 0;
        }
    }

    std::println("Success!");
    return 0;
}